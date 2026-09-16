#include "Training/TrainingAugmentRules.h"

#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentDraft.h"
#include "AbilitySystemComponent.h"
#include "Player/ShooterPlayerState.h"
#include "Weapon/WeaponInventoryComponent.h"

#define LOCTEXT_NAMESPACE "TrainingAugmentRules"

bool TrainingAugmentRules::GetAvailabilityTags(const AShooterPlayerState* PlayerState,
    FGameplayTagContainer& OutTags)
{
    OutTags.Reset();
    const UAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
    const UWeaponInventoryComponent* Weapons = PlayerState ? PlayerState->GetWeaponInventory() : nullptr;
    if (!ASC || !Weapons) { return false; }

    ASC->GetOwnedGameplayTags(OutTags);
    // 이전 Pawn의 무기 태그 대신 현재 인벤토리 기준으로 다시 구성한다.
    const FGameplayTag WeaponUnlockRoot = FGameplayTag::RequestGameplayTag(TEXT("Unlock.Weapon"), false);
    OutTags.RemoveTags(OutTags.Filter(FGameplayTagContainer(WeaponUnlockRoot)));
    FGameplayTagContainer WeaponTags;
    Weapons->GetAugmentUnlockTags(WeaponTags);
    OutTags.AppendTags(WeaponTags);
    return true;
}

bool TrainingAugmentRules::IsCatalogEligible(const UAugmentDefinition& Definition,
    const FGameplayTagContainer& UnlockTags)
{
    return Definition.AugmentCategory != EAugmentCategory::None
        && Definition.Tier < EAugmentTier::MAX
        && Definition.LineTag.IsValid() && Definition.GetLevelData()
        && RogueAugmentDraft::MeetsUnlockRequirements(Definition, UnlockTags);
}

TrainingAugmentRules::EToggleResult TrainingAugmentRules::Toggle(
    TArray<FPrimaryAssetId>& Selection, const FPrimaryAssetId& Id)
{
    if (!Id.IsValid()) { return EToggleResult::InvalidId; }
    if (Selection.RemoveSingle(Id) > 0) { return EToggleResult::Deselected; }
    if (Selection.Num() >= MaximumSelection) { return EToggleResult::LimitReached; }
    Selection.Add(Id);
    return EToggleResult::Selected;
}

bool TrainingAugmentRules::ValidateIds(const TArray<FPrimaryAssetId>& Selection, FText& OutFailure)
{
    OutFailure = FText::GetEmpty();
    if (Selection.Num() > MaximumSelection)
    {
        OutFailure = FText::Format(LOCTEXT("Limit", "Select up to {0} augments in total (active + passive)."),
            FText::AsNumber(MaximumSelection));
        return false;
    }
    TSet<FPrimaryAssetId> Seen;
    for (const FPrimaryAssetId& Id : Selection)
    {
        if (!Id.IsValid() || Seen.Contains(Id))
        {
            OutFailure = LOCTEXT("Invalid", "The selection contains an invalid or duplicate augment.");
            return false;
        }
        Seen.Add(Id);
    }
    return true;
}

FGameplayTag TrainingAugmentRules::GetActiveSlot(const int32 ActiveIndex)
{
    static const FName Names[] = {
        TEXT("InputTag.Ability.Slot1"), TEXT("InputTag.Ability.Slot2"), TEXT("InputTag.Ability.Slot3")
    };
    static_assert(UE_ARRAY_COUNT(Names) == MaximumSelection);
    return ActiveIndex >= 0 && ActiveIndex < UE_ARRAY_COUNT(Names)
        ? FGameplayTag::RequestGameplayTag(Names[ActiveIndex], false) : FGameplayTag();
}

FText TrainingAugmentRules::GetSlotLabel(const int32 ActiveIndex)
{
    return ActiveIndex >= 0 && ActiveIndex < MaximumSelection
        ? FText::Format(LOCTEXT("Slot", "SLOT {0}"), FText::AsNumber(ActiveIndex + 1))
        : FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
