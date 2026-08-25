#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VoiceChatSubsystem.generated.h"

class APlayerState;
class UWorld;

UENUM(BlueprintType)
enum class EVoiceChatAudience : uint8
{
	Team,
	Everyone
};

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

	UFUNCTION(BlueprintCallable, Category = "Shooter|Voice")
	void SetVoiceAudience(EVoiceChatAudience NewAudience);

	UFUNCTION(BlueprintPure, Category = "Shooter|Voice")
	EVoiceChatAudience GetVoiceAudience() const { return VoiceAudience; }

	UFUNCTION(BlueprintPure, Category = "Shooter|Voice")
	bool IsPlayerMuted(const APlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "Shooter|Voice")
	bool CanSendVoice(const APlayerState* Speaker) const;

	bool CanSendVoice(const APlayerState* Speaker, EVoiceChatAudience Audience) const;

	UFUNCTION(BlueprintPure, Category = "Shooter|Voice")
	bool CanHearVoice(const APlayerState* Listener, const APlayerState* Speaker) const;

	bool CanHearVoice(const APlayerState* Listener, const APlayerState* Speaker, EVoiceChatAudience Audience) const;

	UPROPERTY(BlueprintAssignable, Category = "Shooter|Voice")
	FVoiceMuteChangedSignature OnVoiceMuteChanged;

private:
	EVoiceChatAudience GetEffectiveVoiceAudience(const UWorld* World, EVoiceChatAudience RequestedAudience) const;
	bool ArePlayersOnSameTeam(const APlayerState* Listener, const APlayerState* Speaker) const;

	UPROPERTY()
	EVoiceChatAudience VoiceAudience = EVoiceChatAudience::Team;

	mutable TSet<TWeakObjectPtr<APlayerState>> MutedPlayers;
};
