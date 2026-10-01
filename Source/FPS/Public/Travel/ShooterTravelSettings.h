#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ShooterTravelSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Shooter Travel"))
class FPS_API UShooterTravelSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// 온라인 세션 시작 응답에만 적용한다. 에셋 준비는 완료 또는 실패 응답을 기다린다.
	UPROPERTY(Config, EditAnywhere, Category = "Loading", meta = (ClampMin = "1.0", Units = "s"))
	float OnlineSessionStartTimeout = 120.0f;

	// 이동 중인 NetDriver 인스턴스에만 적용하고 도착 후 원래 값으로 복구한다.
	UPROPERTY(Config, EditAnywhere, Category = "Network", meta = (ClampMin = "1.0", Units = "s"))
	float LoadingConnectionTimeout = 180.0f;
};
