#pragma once

#include "CoreMinimal.h"
#include "Misc/NetworkGuid.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "KillcamReplaySubsystem.generated.h"

class ABaseTeamGameState;
class AShooterPlayerController;
class UDemoNetDriver;
class UWorld;
class UKillcamWidget;

UCLASS(Config = Game)
class FPS_API UKillcamReplaySubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	void SetUpForWorld(UWorld* World);
	void ReleaseWorld(UWorld* World);
	bool IsPlaying() const;

private:
	UFUNCTION()
	void HandlePlayerEliminated(const FShooterEliminationEvent& EliminationEvent);

	void BindGameState();
	void StartRecording();
	void StartPlayback();
	void HandlePlaybackReady(bool bWasSuccessful);
	bool ShowPlayback();
	void StopPlayback();
	void CleanPlaybackWorld();
	void RestoreLiveWorld();
	bool IsSupportedWorld(const UWorld* World) const;
	AShooterPlayerController* GetPlayerController() const;
	UDemoNetDriver* GetPlaybackDemoNetDriver() const;
	AActor* GetReplayActor(const FNetworkGUID& ActorGuid) const;

	UPROPERTY()
	TObjectPtr<ABaseTeamGameState> BoundGameState;

	UPROPERTY(Config)
	TSoftClassPtr<UKillcamWidget> WidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UKillcamWidget> PlaybackWidget;

	UPROPERTY(Transient)
	FShooterEliminationEvent PlaybackElimination;

	TWeakObjectPtr<UWorld> SourceWorld;
	TWeakObjectPtr<AActor> SavedLiveViewTarget;
	FNetworkGUID KillerActorGuid;
	FNetworkGUID VictimActorGuid;
	FTimerHandle PostDeathTimerHandle;
	FTimerHandle PlaybackTimerHandle;
	float DeathDemoTime = 0.0f;
	float PlaybackDuration = 5.0f;
	float PostDeathRecordDuration = 0.5f;
	bool bPlaying = false;
};
