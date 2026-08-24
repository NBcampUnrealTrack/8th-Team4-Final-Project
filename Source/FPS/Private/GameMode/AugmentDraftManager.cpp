#include "GameMode/AugmentDraftManager.h"

#include "Augment/AugmentDraft.h"
#include "Augment/AugmentInventoryComponent.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Augment/AugmentTypes.h"
#include "Augment/RogueAugmentSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMode/BaseRoundGameMode.h"
#include "GameMode/BaseRoundGameState.h"
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
	BuildDraftRules();

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

	const int32 TeamAScore = RoundGameState->GetTeamAScore();
	const int32 TeamBScore = RoundGameState->GetTeamBScore();

	const int32 RoundSeed = MakeRoundSeed(RoundNumber);

	// 등급은 팀 단위로 결정
	EAugmentTier TeamATier = EAugmentTier::Common;
	EAugmentTier TeamBTier = EAugmentTier::Common;
	ResolveTeamTiers(RoundNumber, RoundSeed, TeamAScore, TeamBScore, TeamATier, TeamBTier);

	// 후보를 실제로 받은 참가자만 남긴다
	TArray<TWeakObjectPtr<AShooterPlayerState>> OfferedParticipants;
	OfferedParticipants.Reserve(DraftParticipants.Num());

	for (const TWeakObjectPtr<AShooterPlayerState>& Participant : DraftParticipants)
	{
		AShooterPlayerState* PlayerState = Participant.Get();
		UAugmentInventoryComponent* Inventory = GetInventory(PlayerState);
		if (!IsValid(Inventory))
		{
			CancelDraft();
			return false;
		}

		const bool bIsTeamA = PlayerState->GetTeam() == EGameTeam::TeamA;

		FAugmentDraftContext DraftContext;
		DraftContext.RoundNumber = RoundNumber;
		DraftContext.RoundSeed = RoundSeed;
		DraftContext.TeamScore = bIsTeamA ? TeamAScore : TeamBScore;
		DraftContext.OpponentScore = bIsTeamA ? TeamBScore : TeamAScore;

		const EAugmentTier Tier = bIsTeamA ? TeamATier : TeamBTier;

		// 후보를 못 받은 플레이어를 이 라운드 드래프트에서 제외
		// 증강 종류가 추가되고, 삭제 로직이 생기면 발생하지 않을 것으로 예상
		// 라운드가 열리지 않는 것을 막기 위한 임시 처리
		if (!RollOffersForParticipant(PlayerState, Inventory, DraftContext, Tier, OfferCount))
		{
			UE_LOG(LogAugmentDraftManager, Warning,
				TEXT("%s skips the round %d augment draft."), *GetNameSafe(PlayerState), RoundNumber);
			continue;
		}

		OfferedParticipants.Add(Participant);
	}

	DraftParticipants = MoveTemp(OfferedParticipants);

	UE_LOG(LogAugmentDraftManager, Log,
		TEXT("Round %d augment draft prepared for %d participants. MatchSeed=%d RoundSeed=%d"),
		RoundNumber, DraftParticipants.Num(), MatchSeed, RoundSeed);
	return true;
}

bool UAugmentDraftManager::OpenDraft()
{
	if (!bDraftEnabled || !AreDependenciesReady())
	{
		return false;
	}

	// 후보를 받은 참가자가 없으면 열 드래프트도 없다. 입력도 막지 않는다
	if (DraftParticipants.IsEmpty())
	{
		return true;
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
	if (!bDraftEnabled)
	{
		return false;
	}

	// OpenDraft 가 아무것도 열지 않았으므로 닫을 것도 없다
	if (DraftParticipants.IsEmpty())
	{
		return true;
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
			|| (!Inventory->HasStagedOfferSelection() && !Inventory->AutoSelectOffer()))
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
			Inventory->ClearOffers();
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
		Inventory->RemoveAllAugments();
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
	const uint32 RandomSeed = FMath::Rand32() & 0x7fffffffu;
	MatchSeed = RandomSeed == 0u ? 1 : static_cast<int32>(RandomSeed);
}

int32 UAugmentDraftManager::MakeRoundSeed(const int32 RoundNumber) const
{
	uint32 Seed = HashCombine(static_cast<uint32>(MatchSeed), static_cast<uint32>(FMath::Max(1, RoundNumber)));
	Seed &= 0x7fffffffu;
	return Seed == 0u ? 1 : static_cast<int32>(Seed);
}

void UAugmentDraftManager::BuildDraftRules()
{
	DraftRules = FAugmentDraftRules();

	// 가중치만 프로젝트 세팅에서 가져오고 나머지는 기본값
	if (const URogueAugmentSettings* Settings = GetDefault<URogueAugmentSettings>())
	{
		DraftRules.TierWeights = Settings->TierDraftWeights;
	}
}

int32 UAugmentDraftManager::MakeDraftSeed(
	const AShooterPlayerState* PlayerState,
	const FAugmentDraftContext& DraftContext)
{
	// 시드가 없으면 랜덤
	if (DraftContext.RoundSeed == 0)
	{
		return FMath::Rand();
	}

	const int32 PlayerKey = IsValid(PlayerState) ? PlayerState->GetPlayerId() : 0;

	const uint32 WithPlayer =
		HashCombine(static_cast<uint32>(DraftContext.RoundSeed), static_cast<uint32>(PlayerKey));

	return static_cast<int32>(HashCombine(WithPlayer, static_cast<uint32>(DraftContext.RoundNumber)));
}

// 등급은 팀 단위로 한 번만
void UAugmentDraftManager::ResolveTeamTiers(
	const int32 RoundNumber,
	const int32 RoundSeed,
	const int32 TeamAScore,
	const int32 TeamBScore,
	EAugmentTier& OutTeamATier,
	EAugmentTier& OutTeamBTier) const
{
	// 동점일 경우 한 번만 등급
	if (TeamAScore == TeamBScore)
	{
		OutTeamATier = RogueAugmentDraft::ResolveTier(DraftRules, RoundNumber, RoundSeed, /*bIsBehind*/ false);
		OutTeamBTier = OutTeamATier;
		return;
	}

	// 팀마다 다른 등급이 나오도록 시드 분리
	const int32 SeedA = static_cast<int32>(
		HashCombine(static_cast<uint32>(RoundSeed), static_cast<uint32>(EGameTeam::TeamA)));
	const int32 SeedB = static_cast<int32>(
		HashCombine(static_cast<uint32>(RoundSeed), static_cast<uint32>(EGameTeam::TeamB)));

	// 점수가 낮은 팀에게 등급 가중치 적용
	const bool bIsTeamABehind = TeamAScore < TeamBScore;

	if (bIsTeamABehind)
	{
		OutTeamBTier = RogueAugmentDraft::ResolveTier(DraftRules, RoundNumber, SeedB, /*bIsBehind*/ false);
		OutTeamATier = RogueAugmentDraft::ResolveTier(
			DraftRules, RoundNumber, SeedA, /*bIsBehind*/ true, /*MinimumTier*/ OutTeamBTier);
	}
	else
	{
		OutTeamATier = RogueAugmentDraft::ResolveTier(DraftRules, RoundNumber, SeedA, /*bIsBehind*/ false);
		OutTeamBTier = RogueAugmentDraft::ResolveTier(
			DraftRules, RoundNumber, SeedB, /*bIsBehind*/ true, /*MinimumTier*/ OutTeamATier);
	}
}

bool UAugmentDraftManager::RollOffersForParticipant(
	AShooterPlayerState* PlayerState,
	UAugmentInventoryComponent* Inventory,
	const FAugmentDraftContext& DraftContext,
	const EAugmentTier Tier,
	const int32 OfferCount) const
{
	const UAugmentRegistrySubsystem* Registry = BoundRegistry.Get();
	if (!Registry || !Registry->IsReady() || !IsValid(Inventory) || !IsValid(PlayerState))
	{
		return false;
	}

	const int32 Seed = MakeDraftSeed(PlayerState, DraftContext);

	// 플레이어의 인벤토리 상태 저장 및 등급 저장
	FAugmentDraftEligibility State;
	Inventory->FillDraftEligibility(State);
	State.Tier = Tier;

	// 선택한 등급에 대한 후보 버킷 생성
	TArray<FAugmentInventoryEntry> Bucket;
	RogueAugmentDraft::BuildCandidateBucket(DraftRules, State, Registry->GetDefinitionsByTier(Tier), Bucket);

	// 뽑을 후보가 없는 경우
	if (Bucket.IsEmpty())
	{
		UE_LOG(LogAugmentDraftManager, Warning,
			TEXT("%s has no augment candidates for tier %s in round %d."),
			*GetNameSafe(PlayerState), *UEnum::GetValueAsString(Tier), DraftContext.RoundNumber);
		return false;
	}

	// 버킷에서 랜덤으로 후보와 리롤 예비를 선택
	const FAugmentOfferResult Offer =
		RogueAugmentDraft::PickOffers(Bucket, OfferCount, Seed, State.CardsPerSlot);

	// 버킷의 증강이 부족해 요청한 수량보다 적게 선택된 경우, 로그를 남김
	if (Offer.UnfilledSlots > 0)
	{
		UE_LOG(LogAugmentDraftManager, Warning,
			TEXT("%s could only fill %d of %d augment offers for round %d (tier %s)."),
			*GetNameSafe(PlayerState), Offer.SlotCards.Num(), OfferCount, DraftContext.RoundNumber,
			*UEnum::GetValueAsString(Tier));
	}

	// 선택된 후보를 플레이어의 인벤토리에 등록
	if (!Inventory->SetOffers(Offer.SlotCards, Seed))
	{
		return false;
	}

	// 등장한 증강은 다시 등장하지 못하도록 저장
	Inventory->MarkOffersSeen(Inventory->GetOffers());

	return true;
}

bool UAugmentDraftManager::SetSelectionGate(const bool bEnabled) const
{
	bool bAllUpdated = true;
	for (const TWeakObjectPtr<AShooterPlayerState>& Participant : DraftParticipants)
	{
		UAugmentInventoryComponent* Inventory = GetInventory(Participant.Get());
		bAllUpdated &= IsValid(Inventory) && Inventory->SetOfferSelectionEnabled(bEnabled);
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
