#pragma once

#include "CoreMinimal.h"
#include "Misc/NetworkGuid.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "KillcamReplaySubsystem.generated.h"

class ABaseTeamGameState;
class APawn;
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
	bool CanSkipPlayback() const;
	void SkipPlayback();
	void CancelForRespawn();
	void HandleOwnedPawnChanged(APawn* NewPawn);

private:
	UFUNCTION()
	void HandlePlayerEliminated(const FShooterEliminationEvent& EliminationEvent);

	UFUNCTION()
	void HandleMatchStateChanged();

	void BindGameState();
	void UnbindGameState();
	bool IsMatchEnded() const;
	void RetrySetup();
	bool IsSetUpComplete() const;
	float GetRequiredBufferSeconds() const;
	void StartRecording();
	void StartPlayback(uint64 Generation, TWeakObjectPtr<UWorld> RequestWorld, TWeakObjectPtr<APawn> VictimPawn);
	void HandlePlaybackReady(bool bWasSuccessful, uint64 Generation, TWeakObjectPtr<UWorld> RequestWorld,
		TWeakObjectPtr<APawn> VictimPawn, TWeakObjectPtr<UDemoNetDriver> PlaybackDriver);
	bool IsCurrentRequest(uint64 Generation, TWeakObjectPtr<UWorld> RequestWorld,
		TWeakObjectPtr<APawn> VictimPawn) const;
	bool CanPlayForCurrentLife() const;
	bool ShowPlayback();
	void StopPlayback();
	void CancelPlayback(bool bRestartRecording);
	void FinishPlaybackCleanup(TWeakObjectPtr<UWorld> RequestWorld,
		TWeakObjectPtr<UDemoNetDriver> PlaybackDriver, uint64 Generation, bool bRestartRecording);

	AKillcamCameraActor* SpawnPlaybackCamera(AActor* KillerActor);
	void DestroyPlaybackCamera();

	AActor* ResolveRestoreViewTarget(AShooterPlayerController* PlayerController) const;
	void CleanPlaybackWorld(UWorld* World, UDemoNetDriver* ExpectedPlaybackDriver);
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

	UPROPERTY(Config)
	float SkipInputGraceSeconds = 0.3f;

	UPROPERTY(Transient)
	TObjectPtr<UKillcamWidget> PlaybackWidget;

	UPROPERTY(Transient)
	TObjectPtr<AKillcamCameraActor> PlaybackCamera;

	UPROPERTY(Transient)
	FShooterEliminationEvent PlaybackElimination;

	TWeakObjectPtr<UWorld> SourceWorld;
	TWeakObjectPtr<UWorld> PlaybackCleanupWorld;
	TWeakObjectPtr<APawn> VictimPawnAtElimination;
	FGuid VictimPawnLifeIdAtElimination;
	TWeakObjectPtr<AActor> SavedLiveViewTarget;
	FNetworkGUID KillerActorGuid;
	FNetworkGUID VictimActorGuid;
	FTimerHandle PostDeathTimerHandle;
	FTimerHandle PlaybackTimerHandle;
	FTimerHandle PlaybackCleanupTimerHandle;
	FTimerHandle SetupRetryTimerHandle;
	float SetupRetryElapsed = 0.0f;
	float DeathDemoTime = 0.0f;
	double PlaybackShownRealTime = 0.0;
	bool bPlaying = false;
	bool bPlaybackRequestPending = false;
	uint64 PlaybackGeneration = 0;
};
