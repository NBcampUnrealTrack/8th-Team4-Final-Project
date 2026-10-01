// ShooterVitalBarLayout.h
#pragma once

#include "CoreMinimal.h"

// 레이아웃 계산 입력
struct FShooterVitalBarLayoutInput
{
	float MaxHealth = 0.f;
	float MaxShield = 0.f;
	float HealthReal = 0.f;
	float HealthTrail = 0.f;
	float ShieldReal = 0.f;
	float ShieldTrail = 0.f;
	float HealthFlash = 0.f;
	float ShieldFlash = 0.f;
	float SegmentValue = 10.f;
};

// 머티리얼에 넘기는 Parameter값
struct FPS_API FShooterVitalBarLayout
{
	float DisplayD = 0.f;
	float SegmentValue = 0.f;
	float EdgeHPSolid = 0.f;
	// 쉴드 시작점
	float EdgeHP = 0.f;
	float EdgeShieldSolid = 0.f;
	float EdgeContent = 0.f;
	float EdgeMissingHP = 0.f;
	float SegmentStep = 0.f;
	bool bHealthChangeIsHeal = false;
	bool bShieldChangeIsHeal = false;
	float HealthFlash = 0.f;
	float ShieldFlash = 0.f;

	static FShooterVitalBarLayout Compute(const FShooterVitalBarLayoutInput& In);

	// 머티리얼 격자 식과 같은 규칙
	int32 GetDividerCount() const;

	bool operator==(const FShooterVitalBarLayout& Other) const;

	bool operator!=(const FShooterVitalBarLayout& Other) const
	{
		return !(*this == Other);
	}
};
