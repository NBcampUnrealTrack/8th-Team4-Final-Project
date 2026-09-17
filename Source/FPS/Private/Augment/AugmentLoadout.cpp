#include "Augment/AugmentInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Tags/GameplayTags.h"

#define LOCTEXT_NAMESPACE "AugmentLoadout"

namespace AugmentLoadout
{
    struct FPreparedGrant
    {
        FAugmentInventoryEntry Entry;
        FAugmentResolvedGrant Resolved;
    };

    bool SameGrant(const FAugmentInventoryEntry& A, const FAugmentInventoryEntry& B)
    {
        return A.DefinitionId == B.DefinitionId && A.ActiveInputSlot == B.ActiveInputSlot;
    }

    bool Prepare(const FAugmentInventoryEntry& Requested, UAugmentDefinition* Definition,
        FPreparedGrant& Out, FText& OutFailure)
    {
        if (!Definition || !Requested.LineTag.IsValid() || Definition->LineTag != Requested.LineTag
            || !RogueAugmentAssembler::ResolveAugment(*Definition, Out.Resolved))
        {
            OutFailure = FText::Format(LOCTEXT("BadAsset", "Cannot load augment {0}. Check its definition, GE and GA assets."),
                FText::FromString(Requested.DefinitionId.ToString()));
            return false;
        }
        Out.Entry = Requested;
        if (Definition->AugmentCategory == EAugmentCategory::Active)
        {
            const FGameplayTag OriginalSlot = Definition->GetActiveInputSlotTag();
            if (!OriginalSlot.IsValid() || !Requested.ActiveInputSlot.IsValid())
            {
                OutFailure = LOCTEXT("MissingSlot", "An active augment has no valid input slot.");
                return false;
            }
            // Remap the resolved copy, not the shared DataAsset or ability CDO.
            for (FAugmentResolvedAbility& Ability : Out.Resolved.Abilities)
            {
                if (Ability.InputTag == OriginalSlot) { Ability.InputTag = Requested.ActiveInputSlot; }
            }
        }
        else if (Requested.ActiveInputSlot.IsValid())
        {
            OutFailure = LOCTEXT("PassiveSlot", "A passive augment must not occupy an active input slot.");
            return false;
        }
        // Permanent stat grants must be removable. An Instant GE could mutate base stats
        // without a handle, making deselection/rollback impossible.
        for (UClass* EffectClass : Out.Resolved.EffectClasses)
        {
            const UGameplayEffect* Effect = EffectClass->GetDefaultObject<UGameplayEffect>();
            if (!Effect || Effect->DurationPolicy != EGameplayEffectDurationType::Infinite)
            {
                OutFailure = FText::Format(LOCTEXT("NotReversible",
                    "{0}: GrantedEffects must use Infinite duration to support loadout removal."), Definition->DisplayName);
                return false;
            }
        }
        return true;
    }

    bool Complete(const FPreparedGrant& Grant, const FAugmentGrantHandles& Handles)
    {
        return Handles.EffectHandles.Num() == Grant.Resolved.EffectClasses.Num()
            && Handles.AbilityHandles.Num() == Grant.Resolved.Abilities.Num();
    }
}

bool UAugmentInventoryComponent::ReplaceLoadout(
    const TArray<FAugmentInventoryEntry>& RequestedEntries, FText& OutFailure)
{
    using namespace AugmentLoadout;
    OutFailure = FText::GetEmpty();
    UAugmentRegistrySubsystem* Registry = GetRegistry();
    if (!HasServerAuthority(TEXT("loadout replacement")) || !IsValid(AbilitySystemComponent)
        || !Registry || !Registry->IsReady())
    {
        OutFailure = LOCTEXT("NotReady", "The player's ability system or augment catalog is not ready.");
        return false;
    }

    TArray<FPreparedGrant> Desired;
    TSet<FGameplayTag> Lines;
    TSet<FGameplayTag> Slots;
    for (const FAugmentInventoryEntry& Requested : RequestedEntries)
    {
        if (Lines.Contains(Requested.LineTag)
            || (Requested.ActiveInputSlot.IsValid() && Slots.Contains(Requested.ActiveInputSlot)))
        {
            OutFailure = LOCTEXT("Duplicate", "The loadout contains duplicate augment lines or active slots.");
            return false;
        }
        FPreparedGrant Prepared;
        if (!Prepare(Requested, ResolveDefinition(Requested.DefinitionId), Prepared, OutFailure)) { return false; }
        Lines.Add(Requested.LineTag);
        if (Requested.ActiveInputSlot.IsValid()) { Slots.Add(Requested.ActiveInputSlot); }
        Desired.Add(MoveTemp(Prepared));
    }

    // Resolve the recovery path before touching live grants. Unchanged grants retain
    // their original handles, cooldowns and event subscriptions on repeated Apply.
    const TArray<FAugmentInventoryEntry> PreviousEntries = Entries;
    TArray<FPreparedGrant> Removed;
    for (const FAugmentInventoryEntry& Previous : PreviousEntries)
    {
        if (Desired.ContainsByPredicate([&Previous](const FPreparedGrant& Grant) { return SameGrant(Grant.Entry, Previous); }))
        {
            continue;
        }
        FPreparedGrant Prepared;
        if (!Prepare(Previous, ResolveDefinition(Previous.DefinitionId), Prepared, OutFailure)) { return false; }
        Removed.Add(MoveTemp(Prepared));
    }
    const auto RemoveForReplacement = [this](const FGameplayTag& Line)
    {
        // End an outgoing held/casting ability before removing its spec. ClearAbility alone
        // may defer removal until the active instance ends and leave its tasks/input lock alive.
        if (const FAugmentGrantHandles* Handles = GrantHandlesByLine.Find(Line))
        {
            const TArray<FGameplayAbilitySpecHandle> AbilityHandles = Handles->AbilityHandles;
            for (const FGameplayAbilitySpecHandle& Handle : AbilityHandles)
            {
                AbilitySystemComponent->CancelAbilityHandle(Handle);
            }
        }
        // A departed augment's slot cooldown must not block the replacement ability.
        // Keep every unrelated cooldown/status effect, including unchanged augments.
        const int32 EntryIndex = FindEntryIndexInLine(Line);
        const UAugmentDefinition* Definition = Entries.IsValidIndex(EntryIndex)
            ? ResolveDefinition(Entries[EntryIndex].DefinitionId) : nullptr;
        if (Definition)
        {
            FGameplayTagContainer CooldownTags;
            CooldownTags.AddTag(RogueTags::Cooldown_Augment_Slot1);
            CooldownTags.AddTag(RogueTags::Cooldown_Augment_Slot2);
            CooldownTags.AddTag(RogueTags::Cooldown_Augment_Slot3);
            const TArray<FActiveGameplayEffectHandle> Cooldowns = AbilitySystemComponent->GetActiveEffects(
                FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags));
            for (const FActiveGameplayEffectHandle& Handle : Cooldowns)
            {
                const FActiveGameplayEffect* Effect = AbilitySystemComponent->GetActiveGameplayEffect(Handle);
                if (Effect && Effect->Spec.GetContext().GetSourceObject() == Definition)
                {
                    AbilitySystemComponent->RemoveActiveGameplayEffect(Handle);
                }
            }
        }
        RemoveLineAugment(Line);
    };
    for (const FPreparedGrant& Grant : Removed) { RemoveForReplacement(Grant.Entry.LineTag); }
    RebuildConditionalRuleCache();

    TArray<FGameplayTag> AddedLines;
    bool bApplied = true;
    for (const FPreparedGrant& Grant : Desired)
    {
        if (Entries.ContainsByPredicate([&Grant](const FAugmentInventoryEntry& Entry) { return SameGrant(Entry, Grant.Entry); }))
        {
            continue;
        }
        FAugmentGrantHandles Handles = RogueAugmentAssembler::ApplyResolved(*AbilitySystemComponent, Grant.Resolved);
        if (!Complete(Grant, Handles))
        {
            RogueAugmentAssembler::RemoveAugment(*AbilitySystemComponent, Handles);
            OutFailure = FText::Format(LOCTEXT("GrantFailed", "Could not apply {0}. Check GAS requirements and the Output Log."),
                Grant.Resolved.Definition->DisplayName);
            bApplied = false;
            break;
        }
        GrantHandlesByLine.Add(Grant.Entry.LineTag, MoveTemp(Handles));
        Entries.Add(Grant.Entry);
        AddedLines.Add(Grant.Entry.LineTag);
    }

    if (!bApplied)
    {
        for (const FGameplayTag& Line : AddedLines) { RemoveForReplacement(Line); }
        bool bRestored = true;
        for (const FPreparedGrant& Grant : Removed)
        {
            FAugmentGrantHandles Handles = RogueAugmentAssembler::ApplyResolved(*AbilitySystemComponent, Grant.Resolved);
            if (!Complete(Grant, Handles))
            {
                RogueAugmentAssembler::RemoveAugment(*AbilitySystemComponent, Handles);
                bRestored = false;
                continue;
            }
            GrantHandlesByLine.Add(Grant.Entry.LineTag, MoveTemp(Handles));
            Entries.Add(Grant.Entry);
        }
        if (bRestored) { Entries = PreviousEntries; }
        else
        {
            OutFailure = LOCTEXT("RestoreFailed", "GAS rejected the new loadout and part of the previous loadout could not be restored. Reopen selection and check the Output Log.");
            UE_LOG(LogTemp, Error, TEXT("[AugmentLoadout] Recovery failed for %s"), *GetNameSafe(GetOwner()));
        }
    }
    else
    {
        Entries = RequestedEntries;
        // Manual loadout replacement completes no draft reward: do NOT broadcast
        // OnOfferCommitted, which would make the draft manager serve another reward.
        ClearSelectionTimeout();
        ClearRoundDraftState();
        PendingRewards.Reset();
        OnOffersChanged.Broadcast();
    }
    RebuildConditionalRuleCache();
    OnInventoryChanged.Broadcast();
    GetOwner()->ForceNetUpdate();
    return bApplied;
}

#undef LOCTEXT_NAMESPACE
