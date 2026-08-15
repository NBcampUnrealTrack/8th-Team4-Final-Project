#include "GameFlow/Round/AugmentDraftManager.h"

#include "Augment/AugmentInventoryComponent.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Augment/AugmentTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFlow/Round/BaseRoundGameMode.h"
#include "GameFlow/Round/BaseRoundGameState.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/PlayerState.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/GameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogAugmentDraftManager, Log, All);

void UAugmentDraftManager::Initialize(ABaseRoundGameMode* InOwnerGameMode, const bool bInDraftEnabled)
{
	Shutdown();

	OwnerGameMode = InOwnerGameMode;
	bDraftEnabled = bInDraftEnabled;
	RegenerateMatchSeed();

	if (!bDraftEnabled)
	{
		return;
	}

	UWorld* World = IsValid(InOwnerGameMode) ? InOwnerGameMode->GetWorld() : nullptr;
	UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	UAugmentRegistrySubsystem* Registry = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UAugmentRegistrySubsystem>()
		: nullptr;

	if (!IsValid(Registry))
	{
		UE_LOG(LogAugmentDraftManager, Error, TEXT("Cannot find UAugmentRegistrySubsystem."));
		return;
	}

	BoundRegistry = Registry;
	if (!Registry->IsReady())
	{
		Registry->OnRegistryReady.AddUniqueDynamic(this, &UAugmentDraftManager::HandleRegistryReady);
	}
}

void UAugmentDraftManager::Shutdown()
{
	CancelDraft();

	if (UAugmentRegistrySubsystem* Registry = BoundRegistry.Get())
	{
		Registry->OnRegistryReady.RemoveDynamic(this, &UAugmentDraftManager::HandleRegistryReady);
	}

	BoundRegistry.Reset();
	OwnerGameMode.Reset();
	bDraftEnabled = false;
	OnDependenciesReady.Clear();
}

bool UAugmentDraftManager::HasRequiredDependencies() const
{
	return !bDraftEnabled || BoundRegistry.IsValid();
}

bool UAugmentDraftManager::AreDependenciesReady() const
{
	return !bDraftEnabled || (BoundRegistry.IsValid() && BoundRegistry->IsReady());
}

bool UAugmentDraftManager::PrepareDraft(
	const int32 RoundNumber,
	const int32 OfferCount,
	const TArray<AShooterPlayerState*>& Participants)
{
	if (!bDraftEnabled || !AreDependenciesReady() || RoundNumber <= 0 || OfferCount <= 0 || Participants.IsEmpty())
	{
		return false;
	}

	CancelDraft();

	TSet<AShooterPlayerState*> UniqueParticipants;
	for (AShooterPlayerState* PlayerState : Participants)
	{
		if (!IsValid(PlayerState)
			|| !PlayerState->HasAuthority()
			|| !IsValid(GetInventory(PlayerState))
			|| UniqueParticipants.Contains(PlayerState))
		{
			UE_LOG(LogAugmentDraftManager, Error,
				TEXT("Round %d contains an invalid or duplicate augment draft participant."),
				RoundNumber);
			CancelDraft();
			return false;
		}

		UniqueParticipants.Add(PlayerState);
		DraftParticipants.Add(PlayerState);
	}

	const ABaseRoundGameMode* GameMode = OwnerGameMode.Get();
	const ABaseRoundGameState* RoundGameState = IsValid(GameMode)
		? GameMode->GetGameState<ABaseRoundGameState>()
		: nullptr;
	if (!IsValid(RoundGameState))
	{
		CancelDraft();
		return false;
	}

	const int32 RoundSeed = MakeRoundSeed(RoundNumber);
	for (const TWeakObjectPtr<AShooterPlayerState>& Participant : DraftParticipants)
	{
		AShooterPlayerState* PlayerState = Participant.Get();
		UAugmentInventoryComponent* Inventory = GetInventory(PlayerState);
		if (!IsValid(Inventory))
		{
			CancelDraft();
			return false;
		}

		FAugmentDraftContext DraftContext;
		DraftContext.RoundNumber = RoundNumber;
		DraftContext.RoundSeed = RoundSeed;
		if (PlayerState->GetTeam() == EGameTeam::TeamA)
		{
			DraftContext.TeamScore = RoundGameState->GetTeamAScore();
			DraftContext.OpponentScore = RoundGameState->GetTeamBScore();
		}
		else
		{
			DraftContext.TeamScore = RoundGameState->GetTeamBScore();
			DraftContext.OpponentScore = RoundGameState->GetTeamAScore();
		}

		if (!Inventory->ServerRollOffers(DraftContext, OfferCount)
			|| Inventory->GetOffers().Num() != OfferCount)
		{
			UE_LOG(LogAugmentDraftManager, Error,
				TEXT("%s failed to prepare all %d augment offers for round %d."),
				*GetNameSafe(PlayerState), OfferCount, RoundNumber);
			CancelDraft();
			return false;
		}
	}

	UE_LOG(LogAugmentDraftManager, Log,
		TEXT("Round %d augment draft prepared for %d participants. MatchSeed=%d RoundSeed=%d"),
		RoundNumber, DraftParticipants.Num(), MatchSeed, RoundSeed);
	return true;
}

bool UAugmentDraftManager::OpenDraft()
{
	if (!bDraftEnabled || !AreDependenciesReady() || DraftParticipants.IsEmpty())
	{
		return false;
	}

	if (!SetGameplayInputBlocked(true))
	{
		SetGameplayInputBlocked(false);
		return false;
	}

	if (!SetSelectionGate(true))
	{
		SetSelectionGate(false);
		SetGameplayInputBlocked(false);
		return false;
	}

	return true;
}

bool UAugmentDraftManager::CompleteDraft()
{
	if (!bDraftEnabled || DraftParticipants.IsEmpty())
	{
		return false;
	}

	// 자동 선택과 늦게 도착한 선택 RPC가 경합하지 않도록 먼저 게이트를 닫는다.
	if (!SetSelectionGate(false))
	{
		return false;
	}

	for (const TWeakObjectPtr<AShooterPlayerState>& Participant : DraftParticipants)
	{
		UAugmentInventoryComponent* Inventory = GetInventory(Participant.Get());
		if (!IsValid(Inventory)
			|| (!Inventory->HasStagedOfferSelection() && !Inventory->ServerAutoSelectOffer()))
		{
			return false;
		}
	}

	// 첫 적용 전에 모든 선택이 커밋 가능한지 확인한다.
	for (const TWeakObjectPtr<AShooterPlayerState>& Participant : DraftParticipants)
	{
		const UAugmentInventoryComponent* Inventory = GetInventory(Participant.Get());
		if (!IsValid(Inventory) || !Inventory->CanCommitStagedOffer())
		{
			return false;
		}
	}

	TArray<UAugmentInventoryComponent*> TouchedInventories;
	TouchedInventories.Reserve(DraftParticipants.Num());

	auto RollbackTouchedRewards = [&TouchedInventories]()
	{
		bool bAllRolledBack = true;
		for (int32 Index = TouchedInventories.Num() - 1; Index >= 0; --Index)
		{
			UAugmentInventoryComponent* Inventory = TouchedInventories[Index];
			if (IsValid(Inventory) && !Inventory->RollbackStagedOfferCommit())
			{
				bAllRolledBack = false;
				UE_LOG(LogAugmentDraftManager, Error,
					TEXT("Failed to roll back a draft reward transaction for %s."),
					*GetNameSafe(Inventory->GetOwner()));
			}
		}
		return bAllRolledBack;
	};

	for (const TWeakObjectPtr<AShooterPlayerState>& Participant : DraftParticipants)
	{
		UAugmentInventoryComponent* Inventory = GetInventory(Participant.Get());
		if (!IsValid(Inventory))
		{
			RollbackTouchedRewards();
			return false;
		}

		// 적용 자체가 실패해도 스냅샷 복구가 필요할 수 있으므로 먼저 기록한다.
		TouchedInventories.Add(Inventory);
		if (!Inventory->CommitStagedOffer())
		{
			UE_LOG(LogAugmentDraftManager, Error,
				TEXT("Failed to commit the staged augment for %s after draft validation."),
				*GetNameSafe(Participant.Get()));
			RollbackTouchedRewards();
			return false;
		}
	}

	// 입력 차단 해제까지 성공한 뒤에만 보상 커밋을 확정한다.
	if (!SetGameplayInputBlocked(false))
	{
		UE_LOG(LogAugmentDraftManager, Error,
			TEXT("Failed to clear the augment-draft gameplay input tag."));
		RollbackTouchedRewards();
		return false;
	}

	for (UAugmentInventoryComponent* Inventory : TouchedInventories)
	{
		Inventory->FinalizeStagedOfferCommit();
	}

	DraftParticipants.Reset();
	return true;
}

void UAugmentDraftManager::CancelDraft()
{
	SetSelectionGate(false);
	SetGameplayInputBlocked(false);

	for (const TWeakObjectPtr<AShooterPlayerState>& Participant : DraftParticipants)
	{
		if (UAugmentInventoryComponent* Inventory = GetInventory(Participant.Get()))
		{
			Inventory->ServerClearOffers();
		}
	}

	DraftParticipants.Reset();
}

bool UAugmentDraftManager::IsParticipant(const AShooterPlayerState* PlayerState) const
{
	if (!IsValid(PlayerState))
	{
		return false;
	}

	for (const TWeakObjectPtr<AShooterPlayerState>& Participant : DraftParticipants)
	{
		if (Participant.Get() == PlayerState)
		{
			return true;
		}
	}

	return false;
}

void UAugmentDraftManager::ResetForNewMatch()
{
	CancelDraft();

	TArray<AShooterPlayerState*> PlayerStates;
	GetCurrentPlayerStates(PlayerStates);
	for (AShooterPlayerState* PlayerState : PlayerStates)
	{
		ResetPlayerForNewMatch(PlayerState);
	}

	RegenerateMatchSeed();
}

void UAugmentDraftManager::ResetPlayerForNewMatch(AShooterPlayerState* PlayerState) const
{
	if (!IsValid(PlayerState))
	{
		return;
	}

	if (UAugmentInventoryComponent* Inventory = GetInventory(PlayerState))
	{
		Inventory->ServerRemoveAllAugments();
	}

	SetPlayerGameplayInputBlocked(PlayerState, false);
}

void UAugmentDraftManager::SynchronizeGameplayInputBlock(AShooterPlayerState* PlayerState) const
{
	SetPlayerGameplayInputBlocked(PlayerState, bGameplayInputBlocked);
}

void UAugmentDraftManager::HandleRegistryReady()
{
	if (UAugmentRegistrySubsystem* Registry = BoundRegistry.Get())
	{
		Registry->OnRegistryReady.RemoveDynamic(this, &UAugmentDraftManager::HandleRegistryReady);
	}

	OnDependenciesReady.Broadcast();
}

void UAugmentDraftManager::RegenerateMatchSeed()
{
	MatchSeed = FMath::Rand();
	if (MatchSeed == 0)
	{
		MatchSeed = 1;
	}
}

int32 UAugmentDraftManager::MakeRoundSeed(const int32 RoundNumber) const
{
	uint32 Seed = HashCombine(static_cast<uint32>(MatchSeed), static_cast<uint32>(FMath::Max(1, RoundNumber)));
	Seed &= 0x7fffffffu;
	return Seed == 0u ? 1 : static_cast<int32>(Seed);
}

bool UAugmentDraftManager::SetSelectionGate(const bool bEnabled) const
{
	bool bAllUpdated = true;
	for (const TWeakObjectPtr<AShooterPlayerState>& Participant : DraftParticipants)
	{
		UAugmentInventoryComponent* Inventory = GetInventory(Participant.Get());
		bAllUpdated &= IsValid(Inventory) && Inventory->ServerSetOfferSelectionEnabled(bEnabled);
	}
	return bAllUpdated;
}

bool UAugmentDraftManager::SetGameplayInputBlocked(const bool bBlocked)
{
	bool bAllParticipantsUpdated = !DraftParticipants.IsEmpty() || !bBlocked;
	for (const TWeakObjectPtr<AShooterPlayerState>& Participant : DraftParticipants)
	{
		bAllParticipantsUpdated &= SetPlayerGameplayInputBlocked(Participant.Get(), bBlocked);
	}

	if (!bBlocked)
	{
		// 늦은 입장자는 참가자가 아니지만 드래프트 중 동기화된 태그는 정리한다.
		TArray<AShooterPlayerState*> CurrentPlayerStates;
		GetCurrentPlayerStates(CurrentPlayerStates);
		for (AShooterPlayerState* PlayerState : CurrentPlayerStates)
		{
			if (!IsParticipant(PlayerState))
			{
				SetPlayerGameplayInputBlocked(PlayerState, false);
			}
		}
	}

	bGameplayInputBlocked = bBlocked && bAllParticipantsUpdated;
	return bAllParticipantsUpdated;
}

bool UAugmentDraftManager::SetPlayerGameplayInputBlocked(
	AShooterPlayerState* PlayerState,
	const bool bBlocked) const
{
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority())
	{
		return false;
	}

	URogueAbilitySystemComponent* AbilitySystem = PlayerState->GetRogueAbilitySystemComponent();
	if (!IsValid(AbilitySystem))
	{
		UE_LOG(LogAugmentDraftManager, Error, TEXT("%s has no Rogue AbilitySystemComponent."), *GetNameSafe(PlayerState));
		return false;
	}

	AbilitySystem->SetLooseGameplayTagCount(
		RogueTags::State_Gameplay_InputBlocked_AugmentDraft,
		bBlocked ? 1 : 0,
		EGameplayTagReplicationState::TagOnly);

	return AbilitySystem->HasMatchingGameplayTag(RogueTags::State_Gameplay_InputBlocked_AugmentDraft) == bBlocked;
}

UAugmentInventoryComponent* UAugmentDraftManager::GetInventory(AShooterPlayerState* PlayerState) const
{
	return IsValid(PlayerState) ? PlayerState->GetAugmentInventory() : nullptr;
}

void UAugmentDraftManager::GetCurrentPlayerStates(TArray<AShooterPlayerState*>& OutPlayerStates) const
{
	OutPlayerStates.Reset();

	const ABaseRoundGameMode* GameMode = OwnerGameMode.Get();
	const ABaseRoundGameState* RoundGameState = IsValid(GameMode)
		? GameMode->GetGameState<ABaseRoundGameState>()
		: nullptr;
	if (!IsValid(RoundGameState))
	{
		return;
	}

	for (APlayerState* PlayerState : RoundGameState->PlayerArray)
	{
		if (AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState))
		{
			OutPlayerStates.Add(ShooterPlayerState);
		}
	}
}
