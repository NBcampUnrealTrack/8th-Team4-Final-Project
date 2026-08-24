#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Augment/AugmentDraft.h"
#include "AugmentDraftManager.generated.h"

class ABaseRoundGameMode;
class AShooterPlayerState;
class UAugmentInventoryComponent;
class UAugmentRegistrySubsystem;
struct FAugmentDraftContext;

DECLARE_MULTICAST_DELEGATE(FOnAugmentDraftDependenciesReady);

/**
 * 서버 전용 증강 드래프트 관리자.
 * GameMode에는 Prepare/Open/Complete/Cancel 네 동작만 노출한다.
 */
UCLASS()
class FPS_API UAugmentDraftManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ABaseRoundGameMode* InOwnerGameMode, bool bInDraftEnabled);
	void Shutdown();

	bool HasRequiredDependencies() const;
	bool AreDependenciesReady() const;

	bool PrepareDraft(int32 RoundNumber, int32 OfferCount, const TArray<AShooterPlayerState*>& Participants);
	bool OpenDraft();
	bool CompleteDraft();
	void CancelDraft();
	bool IsParticipant(const AShooterPlayerState* PlayerState) const;

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
	void BuildDraftRules();

	// 각 플레이어의 드래프트 시드 생성
	static int32 MakeDraftSeed(const AShooterPlayerState* PlayerState, const FAugmentDraftContext& DraftContext);

	// 팀 단위 증강 등급 뽑기
	void ResolveTeamTiers(
		int32 RoundNumber,
		int32 RoundSeed,
		int32 TeamAScore,
		int32 TeamBScore,
		EAugmentTier& OutTeamATier,
		EAugmentTier& OutTeamBTier) const;

	// 플레이어의 인벤토리와 드래프트 규칙을 기반으로 증강 후보를 생성
	bool RollOffersForParticipant(
		AShooterPlayerState* PlayerState,
		UAugmentInventoryComponent* Inventory,
		const FAugmentDraftContext& DraftContext,
		EAugmentTier Tier,
		int32 OfferCount) const;

	bool SetSelectionGate(bool bEnabled) const;
	bool SetGameplayInputBlocked(bool bBlocked);
	bool SetPlayerGameplayInputBlocked(AShooterPlayerState* PlayerState, bool bBlocked) const;
	UAugmentInventoryComponent* GetInventory(AShooterPlayerState* PlayerState) const;
	void GetCurrentPlayerStates(TArray<AShooterPlayerState*>& OutPlayerStates) const;

	TWeakObjectPtr<ABaseRoundGameMode> OwnerGameMode;
	TWeakObjectPtr<UAugmentRegistrySubsystem> BoundRegistry;
	TArray<TWeakObjectPtr<AShooterPlayerState>> DraftParticipants;

	// 프로젝트 설정에서 온 추첨 규칙
	FAugmentDraftRules DraftRules;

	int32 MatchSeed = 1;
	bool bDraftEnabled = false;
	bool bGameplayInputBlocked = false;
};
