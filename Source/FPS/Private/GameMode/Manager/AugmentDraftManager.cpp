#include "GameMode/Manager/AugmentDraftManager.h"

#include "Augment/AugmentDraft.h"
#include "Augment/AugmentDraftProfile.h"
#include "Augment/AugmentInventoryComponent.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Augment/AugmentTypes.h"
#include "Augment/RogueAugmentSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMode/BaseTeamGameMode.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/PlayerState.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/GameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogAugmentDraftManager, Log, All);

void UAugmentDraftManager::Initialize(
	ABaseTeamGameMode* InOwnerGameMode,
	const bool bInDraftEnabled,
	const UAugmentDraftProfile* InDraftProfile)
{
	Shutdown();

	OwnerGameMode = InOwnerGameMode;
	bDraftEnabled = bInDraftEnabled;

	// 프로필이 없으면 프로필 클래스의 기본값을 사용
	const UAugmentDraftProfile* Profile = IsValid(InDraftProfile)
		? InDraftProfile
		: GetDefault<UAugmentDraftProfile>();

	OfferCount = Profile->GetOfferCount();
	SelectionTimeout = Profile->GetSelectionTimeout();

	RegenerateMatchSeed();
	BuildDraftRules(InDraftProfile);

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

void UAugmentDraftManager::CancelDraft()
{
	TArray<AShooterPlayerState*> PlayerStates;
	GetCurrentPlayerStates(PlayerStates);

	for (AShooterPlayerState* PlayerState : PlayerStates)
	{
		StopListeningForCommit(PlayerState);

		if (UAugmentInventoryComponent* Inventory = GetInventory(PlayerState))
		{
			Inventory->ClearOffers();
		}
	}
}

// 모든 플레이어의 대기열에 증강 부여
void UAugmentDraftManager::GrantToAll(
	const EAugmentRewardPool Pool,
	const int32 RoundNumber,
	const FGameplayTag ActiveSlot)
{
	if (!bDraftEnabled)
	{
		return;
	}

	// 액티브 요청인데 슬롯이 없으면 오류
	if (Pool == EAugmentRewardPool::Active && !ActiveSlot.IsValid())
	{
		UE_LOG(LogAugmentDraftManager, Error,
			TEXT("Round %d active reward needs an input slot tag."), RoundNumber);
		return;
	}

	// 등급 선택, 동점이거나 규칙이 팀별 등급을 끄면 동일한 등급
	EAugmentTier TeamATier = EAugmentTier::Common;
	EAugmentTier TeamBTier = EAugmentTier::Common;
	ResolveRewardTiers(RoundNumber, MakeRoundSeed(RoundNumber), TeamATier, TeamBTier);

	// 증강 풀 등록
	FAugmentPendingReward Reward;
	Reward.Pool = Pool;
	Reward.RoundNumber = RoundNumber;

	// 액티브 증강일 경우 슬롯 등록
	if (Pool == EAugmentRewardPool::Active)
	{
		Reward.ActiveSlot = ActiveSlot;
	}

	// 모든 플레이어의 대기열에 증강 등록
	TArray<AShooterPlayerState*> PlayerStates;
	GetCurrentPlayerStates(PlayerStates);

	for (AShooterPlayerState* PlayerState : PlayerStates)
	{
		UAugmentInventoryComponent* Inventory = GetInventory(PlayerState);
		if (!IsValid(Inventory))
		{
			continue;
		}
		Reward.Tier = PlayerState->GetTeam() == EGameTeam::TeamA ? TeamATier : TeamBTier;

		Inventory->PushPendingReward(Reward);
	}

	UE_LOG(LogAugmentDraftManager, Log,
		TEXT("Round %d reward granted to %d players. Pool=%s TeamA=%s TeamB=%s Slot=%s"),
		RoundNumber, PlayerStates.Num(),
		*UEnum::GetValueAsString(Pool),
		*UEnum::GetValueAsString(TeamATier),
		*UEnum::GetValueAsString(TeamBTier),
		*ActiveSlot.ToString());
}

// 플레이어의 대기열에서 다음 보상을 제시
bool UAugmentDraftManager::ServeNextReward(AShooterPlayerState* PlayerState)
{
	if (!bDraftEnabled || !AreDependenciesReady() || OfferCount <= 0)
	{
		return false;
	}

	UAugmentInventoryComponent* Inventory = GetInventory(PlayerState);
	if (!IsValid(Inventory))
	{
		return false;
	}

	if (!Inventory->GetOffers().IsEmpty())
	{
		return false;
	}

	for (;;)
	{
		const FAugmentPendingReward* Pending = Inventory->PeekPendingReward();
		if (!Pending)
		{
			return false;
		}

		FAugmentDraftContext DraftContext;
		DraftContext.RoundNumber = Pending->RoundNumber;
		DraftContext.RoundSeed = MakeRoundSeed(Pending->RoundNumber);

		if (!RollOffersForParticipant(PlayerState, Inventory, DraftContext,
			Pending->Pool, Pending->Tier, Pending->ActiveSlot, OfferCount))
		{
			UE_LOG(LogAugmentDraftManager, Warning,
				TEXT("%s has no candidates for the pending reward. Skipping it."),
				*GetNameSafe(PlayerState));

			Inventory->PopPendingReward();
			continue;
		}

		// 대기열이 남아있으면 다음 보상도 제시하도록 이벤트 연결
		if (!Inventory->OnOfferCommitted.IsBoundToObject(this))
		{
			Inventory->OnOfferCommitted.AddUObject(this, &UAugmentDraftManager::HandleOfferCommitted);
		}

		Inventory->SetOfferSelectionEnabled(true);
		Inventory->StartSelectionTimeout(SelectionTimeout);
		return true;
	}
}

// 플레이어의 대기열에서 증강을 강제로 선택
void UAugmentDraftManager::ForceResolveOffers(AShooterPlayerState* PlayerState)
{
	UAugmentInventoryComponent* Inventory = GetInventory(PlayerState);
	if (!IsValid(Inventory))
	{
		return;
	}

	if (!Inventory->AutoSelectOffer())
	{
		UE_LOG(LogAugmentDraftManager, Error,
			TEXT("Failed to resolve the augment selection for %s."), *GetNameSafe(PlayerState));
	}
}

void UAugmentDraftManager::HandleOfferCommitted(UAugmentInventoryComponent* Inventory)
{
	if (!IsValid(Inventory))
	{
		return;
	}
	
	// 대기열이 남아있으면 다음 보상 제시
	ServeNextReward(Cast<AShooterPlayerState>(Inventory->GetOwner()));
}

void UAugmentDraftManager::StopListeningForCommit(AShooterPlayerState* PlayerState) const
{
	if (UAugmentInventoryComponent* Inventory = GetInventory(PlayerState))
	{
		Inventory->OnOfferCommitted.RemoveAll(this);
	}
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

	StopListeningForCommit(PlayerState);

	if (UAugmentInventoryComponent* Inventory = GetInventory(PlayerState))
	{
		Inventory->RemoveAllAugments();
	}

	ClearPlayerGameplayInputBlock(PlayerState);
}

// 드래프트는 입력을 차단하지 않음
void UAugmentDraftManager::SynchronizeGameplayInputBlock(AShooterPlayerState* PlayerState) const
{
	ClearPlayerGameplayInputBlock(PlayerState);
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

void UAugmentDraftManager::BuildDraftRules(const UAugmentDraftProfile* Profile)
{	
	// 프로필이 있으면 프로필을 우선
	if (IsValid(Profile))
	{
		DraftRules = Profile->GetRules();
		return;
	}

	DraftRules = FAugmentDraftRules();

	// 프로필이 없으면 프로젝트 설정의 등급 가중치만 적용
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
void UAugmentDraftManager::ResolveRewardTiers(
	const int32 RoundNumber,
	const int32 RoundSeed,
	EAugmentTier& OutTeamATier,
	EAugmentTier& OutTeamBTier) const
{
	int32 TeamAScore = 0;
	int32 TeamBScore = 0;

	// 팀별로 굴릴 때만 점수를 가져옴
	if (DraftRules.bPerTeamTiers)
	{
		const ABaseTeamGameMode* GameMode = OwnerGameMode.Get();
		if (const ABaseTeamGameState* TeamGameState = IsValid(GameMode)
			? GameMode->GetGameState<ABaseTeamGameState>()
			: nullptr)
		{
			TeamAScore = TeamGameState->GetTeamAScore();
			TeamBScore = TeamGameState->GetTeamBScore();
		}
	}

	// 팀별로 굴리지 않거나 동점이면 한 번만 굴려 양 팀 공통
	if (!DraftRules.bPerTeamTiers || TeamAScore == TeamBScore)
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
	const EAugmentRewardPool Pool,
	const EAugmentTier Tier,
	const FGameplayTag RequiredActiveSlot,
	const int32 InOfferCount) const
{
	const UAugmentRegistrySubsystem* Registry = BoundRegistry.Get();
	if (!Registry || !Registry->IsReady() || !IsValid(Inventory) || !IsValid(PlayerState))
	{
		return false;
	}

	const int32 Seed = MakeDraftSeed(PlayerState, DraftContext);

	// 플레이어의 인벤토리 상태와 이번 보상의 풀
	FAugmentDraftEligibility State;
	Inventory->FillDraftEligibility(State);
	State.Pool = Pool;
	State.Tier = Tier;
	State.RequiredActiveSlot = RequiredActiveSlot;

	// 패시브, 액티브 증강 여부
	const TArray<UAugmentDefinition*>& Candidates = Pool == EAugmentRewardPool::Active
		? Registry->GetActiveDefinitions()
		: Registry->GetDefinitionsByTier(Tier);

	TArray<FAugmentInventoryEntry> Bucket;
	RogueAugmentDraft::BuildCandidateBucket(DraftRules, State, Candidates, Bucket);

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
		RogueAugmentDraft::PickOffers(Bucket, InOfferCount, Seed, State.CardsPerSlot);

	// 버킷의 증강이 부족해 요청한 수량보다 적게 선택된 경우, 로그를 남김
	if (Offer.UnfilledSlots > 0)
	{
		UE_LOG(LogAugmentDraftManager, Warning,
			TEXT("%s could only fill %d of %d augment offers for round %d (tier %s)."),
			*GetNameSafe(PlayerState), Offer.SlotCards.Num(), InOfferCount, DraftContext.RoundNumber,
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

void UAugmentDraftManager::ClearPlayerGameplayInputBlock(AShooterPlayerState* PlayerState) const
{
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority())
	{
		return;
	}

	if (URogueAbilitySystemComponent* AbilitySystem = PlayerState->GetRogueAbilitySystemComponent())
	{
		AbilitySystem->SetLooseGameplayTagCount(
			RogueTags::State_Gameplay_InputBlocked_AugmentDraft,
			0,
			EGameplayTagReplicationState::TagOnly);
	}
}

UAugmentInventoryComponent* UAugmentDraftManager::GetInventory(AShooterPlayerState* PlayerState) const
{
	return IsValid(PlayerState) ? PlayerState->GetAugmentInventory() : nullptr;
}

void UAugmentDraftManager::GetCurrentPlayerStates(TArray<AShooterPlayerState*>& OutPlayerStates) const
{
	OutPlayerStates.Reset();

	const ABaseTeamGameMode* GameMode = OwnerGameMode.Get();
	const ABaseTeamGameState* TeamGameState = IsValid(GameMode)
		? GameMode->GetGameState<ABaseTeamGameState>()
		: nullptr;
	if (!IsValid(TeamGameState))
	{
		return;
	}

	for (APlayerState* PlayerState : TeamGameState->PlayerArray)
	{
		if (AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState))
		{
			OutPlayerStates.Add(ShooterPlayerState);
		}
	}
}
