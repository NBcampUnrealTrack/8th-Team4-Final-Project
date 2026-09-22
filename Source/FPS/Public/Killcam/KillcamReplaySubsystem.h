#pragma once

#include "CoreMinimal.h"
#include "Misc/NetworkGuid.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "KillcamReplaySubsystem.generated.h"

class ABaseTeamGameState;
class AKillcamCameraActor;
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

	static bool IsLiveWorldActor(const AActor* Actor);

	void SetUpForWorld(UWorld* World);
	void ReleaseWorld(UWorld* World);
	bool IsPlaying() const;

private:
	UFUNCTION()
	void HandlePlayerEliminated(const FShooterEliminationEvent& EliminationEvent);

	void BindGameState();
	void RetrySetup();
	bool IsSetUpComplete() const;
	float GetRequiredBufferSeconds() const;
	void StartRecording();
	void StartPlayback();
	void HandlePlaybackReady(bool bWasSuccessful);
	bool ShowPlayback();
	void StopPlayback();

	AKillcamCameraActor* SpawnPlaybackCamera(AActor* KillerActor);
	void DestroyPlaybackCamera();

	AActor* ResolveRestoreViewTarget(AShooterPlayerController* PlayerController) const;
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

	UPROPERTY(Config)
	TSoftClassPtr<AKillcamCameraActor> CameraClass;

	UPROPERTY(Config)
	float PlaybackDuration = 5.0f;

	UPROPERTY(Config)
	float PostDeathRecordDuration = 0.5f;

	UPROPERTY(Config)
	float ReplayBufferSeconds = 10.0f;

	UPROPERTY(Transient)
	TObjectPtr<UKillcamWidget> PlaybackWidget;

	UPROPERTY(Transient)
	TObjectPtr<AKillcamCameraActor> PlaybackCamera;

	UPROPERTY(Transient)
	FShooterEliminationEvent PlaybackElimination;

	TWeakObjectPtr<UWorld> SourceWorld;
	TWeakObjectPtr<AActor> SavedLiveViewTarget;
	FNetworkGUID KillerActorGuid;
	FNetworkGUID VictimActorGuid;
	FTimerHandle PostDeathTimerHandle;
	FTimerHandle PlaybackTimerHandle;
	FTimerHandle SetupRetryTimerHandle;
	float SetupRetryElapsed = 0.0f;
	float DeathDemoTime = 0.0f;
	bool bPlaying = false;
};
