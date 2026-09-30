#pragma once

#include "CoreMinimal.h"
#include "GameMode/Manager/DominationManagerBase.h"
#include "GameMode/Types/DominationTypes.h"
#include "DominationAnnouncerManager.generated.h"

class ADominationGameMode;
class ASystemAnnouncer;

// 서버의 확정된 도미네이션 상태를 방송 ID로 변환한다. 점령 규칙이나 사운드 재생은 수정하지 않는다.
UCLASS()
class FPS_API UDominationAnnouncerManager : public UDominationManagerBase
{
	GENERATED_BODY()

public:
	bool Initialize(ADominationGameMode* InOwnerGameMode, TSubclassOf<ASystemAnnouncer> AnnouncerClass);
	virtual void Shutdown() override;
	ASystemAnnouncer* GetSystemAnnouncer() const;

private:
	UFUNCTION()
	void HandleObjectiveFlowStateChanged();

	UFUNCTION()
	void HandleCaptureStateChanged(FDominationCaptureState NewCaptureState);

	void HandleMatchStarted();

	void Announce(FName AnnouncementId);

	TWeakObjectPtr<ASystemAnnouncer> SystemAnnouncer;
	FDominationObjectiveFlowState PreviousObjectiveFlow;
	FDominationCaptureState PreviousCaptureState;
	EGameTeam LastAnnouncedCaptureTeam = EGameTeam::None;
};
