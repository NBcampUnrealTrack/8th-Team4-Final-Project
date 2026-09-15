#include "Training/TrainingPlayerComponent.h"

#include "AbilitySystemComponent.h"
#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentInventoryComponent.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameMode/TrainingGameMode.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/GameplayTags.h"
#include "Training/TrainingArena.h"
#include "Training/TrainingAugmentRules.h"
#include "Training/TrainingInteractionActor.h"
#include "Training/TrainingSessionSubsystem.h"
#include "Training/UI/TrainingDialogWidget.h"
#include "Weapon/WeaponInventoryComponent.h"

#define LOCTEXT_NAMESPACE "TrainingPlayerAugments"

UAugmentInventoryComponent* UTrainingPlayerComponent::GetAugmentInventory() const
{
    const APlayerController* Participant = GetParticipantController();
    const AShooterPlayerState* State = Participant ? Participant->GetPlayerState<AShooterPlayerState>() : nullptr;
    return State ? State->GetAugmentInventory() : nullptr;
}

bool UTrainingPlayerComponent::GetAugmentAvailabilityTags(FGameplayTagContainer& OutTags) const
{
    OutTags.Reset();
    const APlayerController* Participant = GetParticipantController();
    const AShooterPlayerState* State = Participant ? Participant->GetPlayerState<AShooterPlayerState>() : nullptr;
    const UAbilitySystemComponent* ASC = State ? State->GetAbilitySystemComponent() : nullptr;
    const UWeaponInventoryComponent* Weapons = State ? State->GetWeaponInventory() : nullptr;
    if (!ASC || !Weapons) { return false; }

    ASC->GetOwnedGameplayTags(OutTags);
    FGameplayTagContainer WeaponUnlockTags;
    Weapons->GetAugmentUnlockTags(WeaponUnlockTags);
    OutTags.AppendTags(WeaponUnlockTags);
    return true;
}

TArray<FAugmentInventoryEntry> UTrainingPlayerComponent::GetAugmentSnapshot() const
{
    const UAugmentInventoryComponent* Inventory = GetAugmentInventory();
    return Inventory ? Inventory->GetEntries() : TArray<FAugmentInventoryEntry>();
}

void UTrainingPlayerComponent::SubmitAugments(const int32 RequestId, const TArray<FPrimaryAssetId>& Selection)
{
    ServerApplyAugments(RequestId, Selection);
}

void UTrainingPlayerComponent::ServerApplyAugments_Implementation(
    const int32 RequestId, const TArray<FPrimaryAssetId>& Selection)
{
    FText Failure;
    const auto Reply = [this, RequestId](const bool bSuccess, const FText& Message)
    {
        ClientAugmentResult(RequestId, bSuccess, GetAugmentSnapshot(), Message);
    };
    if (!ValidatePendingRequest(RequestId, Failure) || !TrainingAugmentRules::ValidateIds(Selection, Failure))
    {
        Reply(false, Failure);
        return;
    }
    APlayerController* Participant = GetParticipantController();
    AShooterPlayerState* ParticipantState = Participant ? Participant->GetPlayerState<AShooterPlayerState>() : nullptr;
    const ATrainingGameMode* Mode = GetWorld()->GetAuthGameMode<ATrainingGameMode>();
    const UTrainingSessionSubsystem* Sessions = GetWorld()->GetSubsystem<UTrainingSessionSubsystem>();
    ATrainingArena* Arena = PendingInteractable.IsValid() ? PendingInteractable->Arena.Get() : nullptr;
    UAugmentInventoryComponent* Inventory = GetAugmentInventory();
    UAbilitySystemComponent* ASC = ParticipantState ? ParticipantState->GetAbilitySystemComponent() : nullptr;
    if (!Mode || !Mode->UsesManualAugmentSelection() || PendingKind != ETrainingDialogKind::SelectMode
        || !Sessions || Sessions->GetState() != ETrainingSessionState::Idle
        || !Participant || !Participant->GetPawn() || !Arena || !Arena->IsInMovementArea(*Participant->GetPawn())
        || !Inventory || !ASC || ASC->GetAvatarActor() != Participant->GetPawn()
        || ASC->HasMatchingGameplayTag(RogueTags::State_Combat_IsDead))
    {
        Reply(false, LOCTEXT("Unavailable", "Augments can only be changed at the training terminal, while alive and before starting a session."));
        return;
    }
    UGameInstance* Instance = Participant->GetGameInstance();
    const UAugmentRegistrySubsystem* Registry = Instance ? Instance->GetSubsystem<UAugmentRegistrySubsystem>() : nullptr;
    if (!Registry || !Registry->IsReady())
    {
        Reply(false, LOCTEXT("Loading", "The augment catalog is still loading. Try again shortly."));
        return;
    }

    FGameplayTagContainer UnlockTags;
    if (!GetAugmentAvailabilityTags(UnlockTags))
    {
        Reply(false, LOCTEXT("NoUnlockState", "Your class/weapon unlock state is not ready. Reopen the training terminal."));
        return;
    }
    TArray<FAugmentInventoryEntry> RequestedEntries;
    int32 ActiveIndex = 0;
    for (const FPrimaryAssetId& Id : Selection)
    {
        const UAugmentDefinition* Definition = Registry->FindDefinition(Id);
        if (!Definition || Definition->AugmentCategory == EAugmentCategory::None)
        {
            Reply(false, LOCTEXT("NotCatalogued", "A selected augment is not in the server's augment catalog."));
            return;
        }
        if (!TrainingAugmentRules::IsCatalogEligible(*Definition, UnlockTags))
        {
            Reply(false, FText::Format(LOCTEXT("LockedAugment", "{0} is not available for your current class/weapon unlocks."),
                Definition->DisplayName));
            return;
        }
        FAugmentInventoryEntry Entry = FAugmentInventoryEntry::Make(*Definition);
        if (Definition->AugmentCategory == EAugmentCategory::Active)
        {
            Entry.ActiveInputSlot = TrainingAugmentRules::GetActiveSlot(ActiveIndex++);
            if (!Entry.ActiveInputSlot.IsValid())
            {
                Reply(false, LOCTEXT("MissingInput", "Register InputTag.Ability.Slot1, Slot2 and Slot3 in the project gameplay tags."));
                return;
            }
        }
        RequestedEntries.Add(Entry);
    }
    const bool bApplied = Inventory->ReplaceLoadout(RequestedEntries, Failure);
    Reply(bApplied, Failure);
}

void UTrainingPlayerComponent::ClientAugmentResult_Implementation(const int32 RequestId, const bool bSuccess,
    const TArray<FAugmentInventoryEntry>& AuthoritativeEntries, const FText& Failure)
{
    if (UTrainingDialogWidget* Dialog = ActiveDialog.Get(); Dialog && Dialog->GetRequestId() == RequestId)
    {
        Dialog->ReceiveAugmentResult(bSuccess, AuthoritativeEntries, Failure);
    }
}

#undef LOCTEXT_NAMESPACE
