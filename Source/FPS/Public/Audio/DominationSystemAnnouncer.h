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
	inline const FName MatchStarted(TEXT("Domination.MatchStarted"));
	inline const FName Victory(TEXT("Domination.Victory"));
	inline const FName Defeat(TEXT("Domination.Defeat"));
	// 거점 기믹 확보 안내. 각 클라이언트가 자기 팀 기준(Ally/Enemy)으로 골라 로컬 재생한다.
	inline const FName GimmickReconSecuredAlly(TEXT("Domination.Gimmick.ReconSecured.Ally"));
	inline const FName GimmickReconSecuredEnemy(TEXT("Domination.Gimmick.ReconSecured.Enemy"));
	inline const FName GimmickRelaySecuredAlly(TEXT("Domination.Gimmick.RelaySecured.Ally"));
	inline const FName GimmickRelaySecuredEnemy(TEXT("Domination.Gimmick.RelaySecured.Enemy"));
	inline const FName GimmickRelayTakeoverAlly(TEXT("Domination.Gimmick.RelayTakeover.Ally"));
	inline const FName GimmickRelayTakeoverEnemy(TEXT("Domination.Gimmick.RelayTakeover.Enemy"));
}

// 음성 안내와 폭풍 루프를 같은 방송기가 소유하되 서로의 재생을 막지 않는다.
UCLASS(Blueprintable)
class FPS_API ADominationSystemAnnouncer : public ASystemAnnouncer
{
	GENERATED_BODY()

public:
	ADominationSystemAnnouncer();
	virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

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

	UFUNCTION()
	void HandleMatchStateChanged();

	void BindSandstormState();
	void UpdateSandstormAudio();
	void StopStormLoop();
	void TryAnnounceMatchOutcome();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> StormAudioComponent;

	TWeakObjectPtr<ADominationGameState> BoundGameState;
	double LastWarningPhaseEndServerTime = 0.0;
	bool bMatchOutcomeAnnounced = false;

#if !UE_BUILD_SHIPPING
	float PreviewIntensity = -1.0f;
#endif
};
