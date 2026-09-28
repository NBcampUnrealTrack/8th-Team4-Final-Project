#pragma once

#include "CoreMinimal.h"
#include "Audio/SystemAnnouncer.h"
#include "GameMode/Types/DominationSandstormTypes.h"
#include "DominationSystemAnnouncer.generated.h"

class ADominationGameState;

namespace DominationAnnouncementIds
{
	inline const FName ObjectiveActivated(TEXT("Domination.ObjectiveActivated"));
	inline const FName CaptureStarted(TEXT("Domination.CaptureStarted"));
	inline const FName CaptureCompleted(TEXT("Domination.CaptureCompleted"));
	inline const FName SandstormWarning(TEXT("Domination.SandstormWarning"));
}

// 음성 안내와 폭풍 루프를 같은 방송기가 소유하되 서로의 재생을 막지 않는다.
UCLASS(Blueprintable)
class FPS_API ADominationSystemAnnouncer : public ASystemAnnouncer
{
	GENERATED_BODY()

public:
	ADominationSystemAnnouncer();
	virtual void Tick(float DeltaSeconds) override;

#if !UE_BUILD_SHIPPING
	void SetSandstormPreviewIntensity(float Intensity);
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRep_Owner() override;

	// 반복 재생이 설정된 사운드를 지정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Announcer|Sandstorm")
	TObjectPtr<USoundBase> StormLoopSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Announcer|Sandstorm", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float PeakStormVolume = 0.7f;

	// 비워두면 Shooter Audio Settings의 Sound Effects Sound Class를 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Announcer|Sandstorm")
	TObjectPtr<USoundClass> StormSoundClass;

private:
	UFUNCTION()
	void HandleSandstormStateChanged(const FDominationSandstormState& State);

	UFUNCTION()
	void HandleMatchFlowStateChanged();

	void BindSandstormState();
	void UpdateSandstormAudio();
	void StopStormLoop();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> StormAudioComponent;

	TWeakObjectPtr<ADominationGameState> BoundGameState;
	double LastWarningPhaseEndServerTime = 0.0;

#if !UE_BUILD_SHIPPING
	float PreviewIntensity = -1.0f;
#endif
};
