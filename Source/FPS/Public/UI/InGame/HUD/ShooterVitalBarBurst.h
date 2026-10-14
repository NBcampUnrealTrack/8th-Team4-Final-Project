// ShooterVitalBarBurst.h
#pragma once

#include "CoreMinimal.h"
#include "ShooterVitalBarBurst.generated.h"

// 피격 조각 튜닝값
USTRUCT()
struct FShooterVitalBarBurstSettings
{
	GENERATED_BODY()

	// 조각 칸 (HP 값)
	UPROPERTY(EditDefaultsOnly, Category = "VitalBar|Burst", meta = (ClampMin = "1.0"))
	float CellValue = 5.f;
	// 조각 하나 수명
	UPROPERTY(EditDefaultsOnly, Category = "VitalBar|Burst", meta = (ClampMin = "0.01"))
	float ChunkSeconds = 0.18f;
	// 이웃 조각 시간차
	UPROPERTY(EditDefaultsOnly, Category = "VitalBar|Burst", meta = (ClampMin = "0.0"))
	float StaggerSeconds = 0.065f;
	// 피해 한 번의 시간차 합 상한
	UPROPERTY(EditDefaultsOnly, Category = "VitalBar|Burst", meta = (ClampMin = "0.0"))
	float MaxSpreadSeconds = 0.3f;
	// 조각 이미지 세로 배율
	UPROPERTY(EditDefaultsOnly, Category = "VitalBar|Burst", meta = (ClampMin = "1.0"))
	float QuadHeightScale = 4.f;
};

// 반영 한 번의 전후 값
struct FShooterVitalBarBurstInput
{
	float HealthBefore = 0.f;
	float HealthAfter = 0.f;
	float MaxHealth = 0.f;
	float ShieldBefore = 0.f;
	float ShieldAfter = 0.f;
	float MaxShield = 0.f;
	float DisplayD = 0.f;
	// 반영 직후 쉴드 시작 위치
	float ShieldStart = 0.f;
};

// 이어진 칸 구간 하나
struct FShooterVitalBurstInstance
{
	float FirstCell = 0.f;
	float LastCell = -1.f;
	float Age = 0.f;
	float Stagger = 0.f;
	float EndAge = 0.f;
	bool bActive = false;
};

// 피해로 깨진 칸을 조각으로 내보낸다
struct FPS_API FShooterVitalBarBurst
{
	static constexpr int32 MaxInstances = 6;

	// 이번 반영이 닿은 칸을 방출
	void Emit(const FShooterVitalBarBurstInput& In, const FShooterVitalBarBurstSettings& Settings);

	// 살아 있는 조각이 남았으면 true
	bool Tick(float DeltaSeconds);

	// 조각을 비우고 이미 터진 위치를 현재 값으로
	void Reset(float HealthReal, float ShieldReal);

	bool IsActive() const;
	const FShooterVitalBurstInstance& GetInstance(int32 Index) const;

private:
	void Push(int32 FirstCell, int32 LastCell, float StartDelay, float Stagger, const FShooterVitalBarBurstSettings& Settings);

	FShooterVitalBurstInstance Instances[MaxInstances];

	// 이미 터뜨린 아래 끝
	float HealthFront = 0.f;
	float ShieldFront = 0.f;
};
