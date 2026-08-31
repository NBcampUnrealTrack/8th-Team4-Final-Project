#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Augment/AugmentDraft.h"
#include "AugmentDraftManager.generated.h"

class ABaseTeamGameMode;
class AShooterPlayerState;
class UAugmentDraftProfile;
class UAugmentInventoryComponent;
class UAugmentRegistrySubsystem;
struct FAugmentDraftContext;
struct FAugmentPendingReward;

DECLARE_MULTICAST_DELEGATE(FOnAugmentDraftDependenciesReady);

UCLASS()
class FPS_API UAugmentDraftManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ABaseTeamGameMode* InOwnerGameMode, bool bInDraftEnabled,
		const UAugmentDraftProfile* InDraftProfile = nullptr);
	void Shutdown();

	bool HasRequiredDependencies() const;
	bool AreDependenciesReady() const;

	// 미선택 후보 제거
	void CancelDraft();

	// 모든 플레이어의 대기열에 등록
	// ActiveSlot 은 Pool 이 Active 일 때만 사용
	void GrantToAll(EAugmentRewardPool Pool, int32 RoundNumber, FGameplayTag ActiveSlot = FGameplayTag());

	// 후보가 없는 항목은 건너뛰고 다음 보상을 제시, 제시할 것이 없으면 false
	bool ServeNextReward(AShooterPlayerState* PlayerState);

	// 남은 후보를 자동 선택으로 정리
	void ForceResolveOffers(AShooterPlayerState* PlayerState);

	void ResetForNewMatch();
	void ResetPlayerForNewMatch(AShooterPlayerState* PlayerState) const;
	void SynchronizeGameplayInputBlock(AShooterPlayerState* PlayerState) const;

	FOnAugmentDraftDependenciesReady OnDependenciesReady;

private:
	UFUNCTION()
	void HandleRegistryReady();

	void RegenerateMatchSeed();
	int32 MakeRoundSeed(int32 RoundNumber) const;

	// 등급 가중치 기반 추첨 규칙 생성
	void BuildDraftRules(const UAugmentDraftProfile* Profile);

	// 각 플레이어의 드래프트 시드 생성
	static int32 MakeDraftSeed(const AShooterPlayerState* PlayerState, const FAugmentDraftContext& DraftContext);

	// 이번 증강의 등급, 규칙의 bPerTeamTiers 가 false 면 1회만 굴려 양 팀 공통
	void ResolveRewardTiers(
		int32 RoundNumber,
		int32 RoundSeed,
		EAugmentTier& OutTeamATier,
		EAugmentTier& OutTeamBTier) const;

	// 플레이어의 인벤토리와 드래프트 규칙을 기반으로 증강 후보를 생성
	bool RollOffersForParticipant(
		AShooterPlayerState* PlayerState,
		UAugmentInventoryComponent* Inventory,
		const FAugmentDraftContext& DraftContext,
		EAugmentRewardPool Pool,
		EAugmentTier Tier,
		FGameplayTag RequiredActiveSlot,
		int32 InOfferCount) const;

	// 선택 확정 통보를 받아 남은 대기열을 이어서 제시
	void HandleOfferCommitted(UAugmentInventoryComponent* Inventory);
	void StopListeningForCommit(AShooterPlayerState* PlayerState) const;

	void ClearPlayerGameplayInputBlock(AShooterPlayerState* PlayerState) const;
	UAugmentInventoryComponent* GetInventory(AShooterPlayerState* PlayerState) const;
	void GetCurrentPlayerStates(TArray<AShooterPlayerState*>& OutPlayerStates) const;

	TWeakObjectPtr<ABaseTeamGameMode> OwnerGameMode;
	TWeakObjectPtr<UAugmentRegistrySubsystem> BoundRegistry;

	// 프로젝트 설정에서 온 추첨 규칙
	FAugmentDraftRules DraftRules;

	int32 MatchSeed = 1;
	bool bDraftEnabled = false;

	// Initialize 에서 받은 제시 설정
	int32 OfferCount = 0;
	float SelectionTimeout = 0.f;
};
