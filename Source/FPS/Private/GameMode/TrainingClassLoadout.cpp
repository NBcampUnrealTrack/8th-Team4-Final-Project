#include "GameMode/TrainingGameMode.h"

#include "AbilitySystemComponent.h"
#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentInventoryComponent.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameMode/Manager/DominationLoadoutManager.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Player/DominationPlayerState.h"
#include "Player/TrainingPlayerController.h"
#include "Tags/GameplayTags.h"
#include "Training/TrainingAugmentRules.h"
#include "Training/TrainingSessionSubsystem.h"
#include "Weapon/RogueWeaponSettings.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/WeaponInventoryComponent.h"

#define LOCTEXT_NAMESPACE "TrainingClassLoadout"

bool ATrainingGameMode::RequestTrainingCombatClass(ATrainingPlayerController* Requester,
    const EDominationCombatClass RequestedClass, FText& OutFailure)
{
    OutFailure = FText::GetEmpty();
    ADominationPlayerState* State = IsValid(Requester) ? Requester->GetPlayerState<ADominationPlayerState>() : nullptr;
    AShooterCharacter* PreviousPawn = IsValid(Requester) ? Requester->GetPawn<AShooterCharacter>() : nullptr;
    UAbilitySystemComponent* ASC = State ? State->GetAbilitySystemComponent() : nullptr;
    const UTrainingSessionSubsystem* Sessions = GetWorld()->GetSubsystem<UTrainingSessionSubsystem>();
    if (!HasAuthority() || !bConfigurationReady || !bUseManualAugmentSelection
        || !State || !PreviousPawn || !ASC || ASC->GetAvatarActor() != PreviousPawn
        || Requester->GetWorld() != GetWorld() || !InitializedPlayers.Contains(Requester)
        || !State->IsLoadedIntoMatch() || !State->HasSelectedCombatClass() || !State->IsAlive()
        || State->GetParticipationState() != EPlayerParticipationState::Active
        || ASC->HasMatchingGameplayTag(RogueTags::State_Combat_IsDead)
        || !Sessions || Sessions->GetState() != ETrainingSessionState::Idle)
    {
        OutFailure = LOCTEXT("Unavailable", "Class changes are only available before a training session, while alive.");
        return false;
    }
    if (State->GetCombatClass() == RequestedClass) { return true; }

    const UDominationClassSettings* Settings = GetDefault<UDominationClassSettings>();
    const FDominationCombatClassDefinition* Definition = Settings->FindDefinition(RequestedClass);
    UClass* PawnClass = Definition ? Settings->ResolvePawnClass(RequestedClass) : nullptr;
    UWeaponInventoryComponent* Weapons = State->GetWeaponInventory();
    UAugmentInventoryComponent* Augments = State->GetAugmentInventory();
    const UAugmentRegistrySubsystem* Registry = GetGameInstance()->GetSubsystem<UAugmentRegistrySubsystem>();
    if (!Definition || RequestedClass == EDominationCombatClass::Unselected
        || !PawnClass || PawnClass->HasAnyClassFlags(CLASS_Abstract) || !Weapons || !Augments
        || !UWeaponInventoryComponent::IsPrimaryWeaponTag(Definition->PrimaryWeaponTag)
        || !GetDefault<URogueWeaponSettings>()->ResolveWeaponClass(Definition->PrimaryWeaponTag))
    {
        OutFailure = LOCTEXT("InvalidClass", "The class Pawn or primary weapon is not configured. Check Domination Classes and Weapon Registry.");
        return false;
    }
    if (!Registry || !Registry->IsReady())
    {
        OutFailure = LOCTEXT("RegistryLoading", "The augment catalog is still loading. Try again shortly.");
        return false;
    }

    const EDominationCombatClass PreviousClass = State->GetCombatClass();
    const FWeaponInventorySnapshot PreviousWeapons = Weapons->CaptureSnapshot();
    const TArray<FAugmentInventoryEntry> PreviousAugments = Augments->GetEntries();
    const FTransform SpawnTransform = PreviousPawn->GetActorTransform();
    const FRotator AimRotation = Requester->GetControlRotation();
    const bool bPreviousCollision = PreviousPawn->GetActorEnableCollision();
    const bool bPreviousHidden = PreviousPawn->IsHidden();
    const UCombatComponent* PreviousCombat = PreviousPawn->FindComponentByClass<UCombatComponent>();
    const AWeaponBase* PreviousWeapon = PreviousCombat ? PreviousCombat->GetCurrentWeapon() : nullptr;
    const FGameplayTag PreviousEquippedTag = PreviousWeapon ? PreviousWeapon->GetEquippedStateTag() : FGameplayTag();

    // 기존 Pawn은 새 Pawn·무기·증강의 적용이 모두 성공한 뒤에만 파괴한다.
    PreviousPawn->SetActorEnableCollision(false);
    AShooterCharacter* NewPawn = GetWorld()->SpawnActorDeferred<AShooterCharacter>(PawnClass, SpawnTransform,
        Requester, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
    if (NewPawn)
    {
        NewPawn->AutoPossessPlayer = EAutoReceiveInput::Disabled;
        NewPawn->AutoPossessAI = EAutoPossessAI::Disabled;
        NewPawn->FinishSpawning(SpawnTransform);
    }
    UCombatComponent* NewCombat = IsValid(NewPawn) ? NewPawn->FindComponentByClass<UCombatComponent>() : nullptr;
    if (!NewCombat)
    {
        if (IsValid(NewPawn)) { NewPawn->Destroy(); }
        PreviousPawn->SetActorEnableCollision(bPreviousCollision);
        OutFailure = LOCTEXT("SpawnFailed", "Could not create the selected class here. Check Pawn collision and its CombatComponent.");
        return false;
    }

    ASC->CancelAllAbilities();
    PreviousPawn->GetCharacterMovement()->StopMovementImmediately();
    PreviousPawn->SetActorHiddenInGame(true);
    Requester->Possess(NewPawn);

    bool bApplied = Requester->GetPawn() == NewPawn && ASC->GetAvatarActor() == NewPawn
        && State->TryChangeCombatClass(RequestedClass)
        && LoadoutManager->InitializeCombatClassInventory(State)
        && LoadoutManager->ApplyCombatClassLoadout(State, NewPawn);
    if (bApplied)
    {
        FGameplayTagContainer AvailabilityTags;
        bApplied = TrainingAugmentRules::GetAvailabilityTags(State, AvailabilityTags);
        TArray<FAugmentInventoryEntry> RetainedAugments;
        int32 ActiveIndex = 0;
        for (const FAugmentInventoryEntry& Entry : PreviousAugments)
        {
            const UAugmentDefinition* Augment = Registry->FindDefinition(Entry.DefinitionId);
            if (Augment && TrainingAugmentRules::IsCatalogEligible(*Augment, AvailabilityTags))
            {
                FAugmentInventoryEntry& Retained = RetainedAugments.Add_GetRef(Entry);
                Retained.ActiveInputSlot = Augment->AugmentCategory == EAugmentCategory::Active
                    ? TrainingAugmentRules::GetActiveSlot(ActiveIndex++) : FGameplayTag();
            }
        }
        bApplied = bApplied && Augments->ReplaceLoadout(RetainedAugments, OutFailure);
    }

    if (!bApplied)
    {
        // 최초 선택 화면이나 PlayerStart로 보내지 않고 직전 훈련 상태로 복구한다.
        Requester->Possess(PreviousPawn);
        NewPawn->Destroy();
        State->TryChangeCombatClass(PreviousClass);
        Weapons->ServerRestoreSnapshot(PreviousWeapons);
        State->SyncEquippedWeaponState(PreviousEquippedTag);
        PreviousPawn->SetActorHiddenInGame(bPreviousHidden);
        PreviousPawn->SetActorEnableCollision(bPreviousCollision);
        Requester->SetControlRotation(AimRotation);
        Requester->ClientSetRotation(AimRotation, false);
        FText RecoveryFailure;
        if (!Augments->ReplaceLoadout(PreviousAugments, RecoveryFailure))
        {
            OutFailure = FText::Format(LOCTEXT("RecoveryFailed", "Class change failed. Check the augment loadout: {0}"), RecoveryFailure);
        }
        else if (OutFailure.IsEmpty())
        {
            OutFailure = LOCTEXT("ApplyFailed", "Class change failed. The previous class and loadout have been restored.");
        }
        State->ForceNetUpdate();
        return false;
    }

    NewCombat->SetWeaponAmmoPolicy(EWeaponAmmoPolicy::InfiniteReserve);
    NewPawn->GetCharacterMovement()->StopMovementImmediately();
    PreviousPawn->Destroy();
    Requester->SetControlRotation(AimRotation);
    Requester->ClientSetRotation(AimRotation, false);
    NewCombat->RefreshEquippedWeaponPresentation();
    NewPawn->ForceNetUpdate();
    State->ForceNetUpdate();
    return true;
}

#undef LOCTEXT_NAMESPACE
