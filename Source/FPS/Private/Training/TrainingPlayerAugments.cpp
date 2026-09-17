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
#include "Player/DominationPlayerState.h"
#include "Player/TrainingPlayerController.h"
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
    if (GetOwner() && !GetOwner()->HasAuthority() && bHasClientLoadout)
    {
        OutTags = ClientLoadout.AvailabilityTags;
        return ClientLoadout.bAvailabilityReady;
    }
    const APlayerController* Participant = GetParticipantController();
    return TrainingAugmentRules::GetAvailabilityTags(
        Participant ? Participant->GetPlayerState<AShooterPlayerState>() : nullptr, OutTags);
}

FTrainingLoadoutSnapshot UTrainingPlayerComponent::BuildLoadoutSnapshot() const
{
    FTrainingLoadoutSnapshot Snapshot;
    const APlayerController* Participant = GetParticipantController();
    const ADominationPlayerState* State = Participant ? Participant->GetPlayerState<ADominationPlayerState>() : nullptr;
    const UAugmentInventoryComponent* Inventory = GetAugmentInventory();
    Snapshot.CombatClass = State ? State->GetCombatClass() : EDominationCombatClass::Unselected;
    if (Inventory) { Snapshot.Augments = Inventory->GetEntries(); }
    Snapshot.bAvailabilityReady = TrainingAugmentRules::GetAvailabilityTags(State, Snapshot.AvailabilityTags);
    return Snapshot;
}

bool UTrainingPlayerComponent::ValidateLoadoutRequest(const int32 RequestId, FText& OutFailure) const
{
    if (!ValidatePendingRequest(RequestId, OutFailure)) { return false; }
    const APlayerController* Participant = GetParticipantController();
    const AShooterPlayerState* State = Participant ? Participant->GetPlayerState<AShooterPlayerState>() : nullptr;
    const UAbilitySystemComponent* ASC = State ? State->GetAbilitySystemComponent() : nullptr;
    const ATrainingGameMode* Mode = GetWorld()->GetAuthGameMode<ATrainingGameMode>();
    const UTrainingSessionSubsystem* Sessions = GetWorld()->GetSubsystem<UTrainingSessionSubsystem>();
    const ATrainingArena* Arena = PendingInteractable.IsValid() ? PendingInteractable->Arena.Get() : nullptr;
    if (!Mode || !Mode->UsesManualAugmentSelection() || PendingKind != ETrainingDialogKind::SelectMode
        || !Sessions || Sessions->GetState() != ETrainingSessionState::Idle
        || !Participant || !Participant->GetPawn() || !Arena || !Arena->IsInMovementArea(*Participant->GetPawn())
        || !GetAugmentInventory() || !ASC || ASC->GetAvatarActor() != Participant->GetPawn()
        || !State->IsAlive() || ASC->HasMatchingGameplayTag(RogueTags::State_Combat_IsDead))
    {
        OutFailure = LOCTEXT("LoadoutUnavailable", "Class and augments can only be changed at the training terminal, while alive and before starting a session.");
        return false;
    }
    return true;
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
        ClientAugmentResult(RequestId, bSuccess, BuildLoadoutSnapshot(), Message);
    };
    if (!ValidateLoadoutRequest(RequestId, Failure) || !TrainingAugmentRules::ValidateIds(Selection, Failure))
    {
        Reply(false, Failure);
        return;
    }
    APlayerController* Participant = GetParticipantController();
    UAugmentInventoryComponent* Inventory = GetAugmentInventory();
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
    const FTrainingLoadoutSnapshot& LoadoutSnapshot, const FText& Failure)
{
    if (UTrainingDialogWidget* Dialog = ActiveDialog.Get(); Dialog && Dialog->GetRequestId() == RequestId)
    {
        ClientLoadout = LoadoutSnapshot;
        bHasClientLoadout = true;
        Dialog->ReceiveAugmentResult(bSuccess, LoadoutSnapshot, Failure);
    }
}

void UTrainingPlayerComponent::SubmitCombatClass(const int32 RequestId,
    const EDominationCombatClass RequestedClass)
{
    ServerChangeCombatClass(RequestId, RequestedClass);
}

void UTrainingPlayerComponent::ServerChangeCombatClass_Implementation(const int32 RequestId,
    const EDominationCombatClass RequestedClass)
{
    FText Failure;
    bool bSuccess = false;
    if (ValidateLoadoutRequest(RequestId, Failure))
    {
        ATrainingGameMode* Mode = GetWorld()->GetAuthGameMode<ATrainingGameMode>();
        bSuccess = Mode->RequestTrainingCombatClass(
            Cast<ATrainingPlayerController>(GetParticipantController()), RequestedClass, Failure);
    }
    ClientCombatClassResult(RequestId, bSuccess, BuildLoadoutSnapshot(), Failure);
}

void UTrainingPlayerComponent::ClientCombatClassResult_Implementation(const int32 RequestId, const bool bSuccess,
    const FTrainingLoadoutSnapshot& LoadoutSnapshot, const FText& Failure)
{
    if (UTrainingDialogWidget* Dialog = ActiveDialog.Get(); Dialog && Dialog->GetRequestId() == RequestId)
    {
        ClientLoadout = LoadoutSnapshot;
        bHasClientLoadout = true;
        Dialog->ReceiveCombatClassResult(bSuccess, LoadoutSnapshot, Failure);
    }
}

#undef LOCTEXT_NAMESPACE
