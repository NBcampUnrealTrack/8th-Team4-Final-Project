#pragma once

#include "CoreMinimal.h"
#include "Audio/SystemAnnouncer.h"
#include "DominationSystemAnnouncer.generated.h"

namespace DominationAnnouncementIds
{
	inline const FName ObjectiveActivated(TEXT("Domination.ObjectiveActivated"));
	inline const FName CaptureStarted(TEXT("Domination.CaptureStarted"));
	inline const FName CaptureCompleted(TEXT("Domination.CaptureCompleted"));
}

// 도미네이션의 기본 음성 항목만 제공한다. 판정과 재생 로직은 각각 매니저와 부모 클래스가 맡는다.
UCLASS(Blueprintable)
class FPS_API ADominationSystemAnnouncer : public ASystemAnnouncer
{
	GENERATED_BODY()

public:
	ADominationSystemAnnouncer();
};
