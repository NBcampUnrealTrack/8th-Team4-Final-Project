#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AugmentDraftManager.generated.h"

class ABaseRoundGameMode;
class AShooterPlayerState;
class UAugmentInventoryComponent;
class UAugmentRegistrySubsystem;

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
	bool SetSelectionGate(bool bEnabled) const;
	bool SetGameplayInputBlocked(bool bBlocked);
	bool SetPlayerGameplayInputBlocked(AShooterPlayerState* PlayerState, bool bBlocked) const;
	UAugmentInventoryComponent* GetInventory(AShooterPlayerState* PlayerState) const;
	void GetCurrentPlayerStates(TArray<AShooterPlayerState*>& OutPlayerStates) const;

	TWeakObjectPtr<ABaseRoundGameMode> OwnerGameMode;
	TWeakObjectPtr<UAugmentRegistrySubsystem> BoundRegistry;
	TArray<TWeakObjectPtr<AShooterPlayerState>> DraftParticipants;
	int32 MatchSeed = 1;
	bool bDraftEnabled = false;
	bool bGameplayInputBlocked = false;
};
