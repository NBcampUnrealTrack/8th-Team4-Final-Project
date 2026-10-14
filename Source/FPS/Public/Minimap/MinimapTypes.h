#pragma once

#include "CoreMinimal.h"
#include "MinimapTypes.generated.h"

UENUM(BlueprintType)
enum class EShooterMapMode : uint8
{
	// 좌상단 원형 미니맵. 로컬 플레이어를 중심으로 주변만 보여준다.
	Minimap UMETA(DisplayName = "Minimap"),
	// M키 전체 맵. 맵 전체를 화면에 맞춰 보여준다.
	FullMap UMETA(DisplayName = "Full Map")
};

// ShooterPingLimits(PingTypes.h)와 동일한 네이밍 컨벤션을 따른다.
namespace ShooterMinimapLimits
{
	// 서버가 "팀원이 적을 보고 있는지"를 재판정하는 주기. Tick 대신 이 주기의 타이머로만 돈다.
	constexpr float SpotCheckIntervalSeconds = 0.2f;

	// 이 거리보다 먼 적은 시야 안이어도 미니맵에 띄우지 않는다.
	constexpr float SpotMaxDistance = 8000.f;

	// 관측자 시선 기준 좌우 반각. 화면 FOV(90)보다 살짝 넓게 잡아 가장자리에 걸친 적도 잡는다.
	constexpr float SpotHalfFovDegrees = 50.f;
}
