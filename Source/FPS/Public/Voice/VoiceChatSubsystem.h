#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VoiceChatSubsystem.generated.h"

class APlayerState;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVoiceMuteChangedSignature, APlayerState*, PlayerState, bool, bMuted);

UCLASS()
class FPS_API UVoiceChatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Shooter|Voice")
	void SetPlayerMuted(APlayerState* PlayerState, bool bMuted);

	UFUNCTION(BlueprintCallable, Category = "Shooter|Voice")
	void TogglePlayerMuted(APlayerState* PlayerState);

	UFUNCTION(BlueprintPure, Category = "Shooter|Voice")
	bool IsPlayerMuted(const APlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Shooter|Voice")
	bool CanSendVoice(const APlayerState* Speaker) const;

	UFUNCTION(BlueprintPure, Category = "Shooter|Voice")
	bool CanHearVoice(const APlayerState* Listener, const APlayerState* Speaker) const;

	UPROPERTY(BlueprintAssignable, Category = "Shooter|Voice")
	FVoiceMuteChangedSignature OnVoiceMuteChanged;

private:
	bool IsTeamVoiceOnly(const UWorld* World) const;
	bool ArePlayersOnSameTeam(const APlayerState* Listener, const APlayerState* Speaker) const;

	mutable TSet<TWeakObjectPtr<APlayerState>> MutedPlayers;
};
