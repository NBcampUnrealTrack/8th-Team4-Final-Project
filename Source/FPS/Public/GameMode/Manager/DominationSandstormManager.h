#pragma once

#include "CoreMinimal.h"
#include "GameMode/Manager/DominationManagerBase.h"
#include "GameMode/Types/DominationSandstormTypes.h"
#include "TimerManager.h"
#include "DominationSandstormManager.generated.h"

class UDominationSandstormSettings;

UCLASS()
class FPS_API UDominationSandstormManager : public UDominationManagerBase
{
	GENERATED_BODY()

public:
	bool Initialize(ADominationGameMode* InOwnerGameMode, UDominationSandstormSettings* InSettings);
	virtual void Shutdown() override;
	void StartScheduling();
	void StopStormFlow();
	void ForceStorm();

private:
	void ScheduleNextStorm(bool bInitialStorm);
	void AdvancePhase();
	void BeginPhase(EDominationSandstormPhase Phase, float Duration);

	UPROPERTY(Transient)
	TObjectPtr<UDominationSandstormSettings> Settings;

	FTimerHandle PhaseTimer;
	bool bRunning = false;
};
