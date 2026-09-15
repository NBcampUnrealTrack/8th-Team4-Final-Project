#pragma once

#include "CoreMinimal.h"
#include "PingTypes.generated.h"

UENUM(BlueprintType)
enum class EShooterPingType : uint8
{
	// 크로스헤어가 지형/오브젝트를 맞춤
	Location UMETA(DisplayName = "Location"),
	// 크로스헤어가 적 캐릭터를 맞춤
	Enemy UMETA(DisplayName = "Enemy")
};

USTRUCT(BlueprintType)
struct FPingEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ping")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Ping")
	EShooterPingType Type = EShooterPingType::Location;
};

// ShooterChatLimits(ChatTypes.h)와 동일한 네이밍 컨벤션을 따른다.
namespace ShooterPingLimits
{
	constexpr float MinSendIntervalSeconds = 1.5f;
	constexpr float PingLifetimeSeconds = 5.0f;
	constexpr float TraceDistance = 10000.f;

	// 크로스헤어가 캐릭터를 정확히 안 맞춰도 적 핑으로 인정해줄 여유 반경. 순수 라인트레이스 대신
	// 이 반경의 스피어로 스윕해서, 살짝 빗나가도 적 캐릭터 콜리전에 걸리면 Enemy로 판정한다.
	constexpr float EnemyPingToleranceRadius = 40.f;
}
