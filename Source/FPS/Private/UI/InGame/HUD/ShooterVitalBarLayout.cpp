// ShooterVitalBarLayout.cpp
#include "UI/InGame/HUD/ShooterVitalBarLayout.h"

namespace ShooterVitalBarLayoutRules
{
	// Display 예외처리
	constexpr float DividerEndEpsilon = 1.0e-4f;
}

FShooterVitalBarLayout FShooterVitalBarLayout::Compute(const FShooterVitalBarLayoutInput& In)
{
	FShooterVitalBarLayout Out;

	const float HealthEnd = FMath::Max(In.HealthReal, In.HealthTrail);
	const float ShieldEnd = FMath::Max(In.ShieldReal, In.ShieldTrail);

	// Player Max값 표시
	Out.DisplayD = FMath::Max(In.MaxHealth, HealthEnd) + FMath::Max(In.MaxShield, ShieldEnd);
	Out.SegmentValue = In.SegmentValue;
	Out.bHealthChangeIsHeal = In.HealthReal > In.HealthTrail;
	Out.bShieldChangeIsHeal = In.ShieldReal > In.ShieldTrail;
	Out.HealthFlash = In.HealthFlash;
	Out.ShieldFlash = In.ShieldFlash;	

	// 값을 받기 전
	if (Out.DisplayD <= 0.f)
	{
		return Out;
	}

	const float ContentEnd = HealthEnd + ShieldEnd;
	const float MissingHealth = FMath::Max(0.f, In.MaxHealth - HealthEnd);

	Out.EdgeHPSolid = FMath::Min(In.HealthReal, In.HealthTrail) / Out.DisplayD;
	Out.EdgeHP = HealthEnd / Out.DisplayD;
	Out.EdgeShieldSolid = (HealthEnd + FMath::Min(In.ShieldReal, In.ShieldTrail)) / Out.DisplayD;
	Out.EdgeContent = ContentEnd / Out.DisplayD;
	Out.EdgeMissingHP = (ContentEnd + MissingHealth) / Out.DisplayD;
	Out.SegmentStep = (In.SegmentValue > 0.f) ? In.SegmentValue / Out.DisplayD : 0.f;

	return Out;
}

int32 FShooterVitalBarLayout::GetDividerCount() const
{
	if (SegmentStep <= 0.f)
	{
		return 0;
	}

	int32 Count = 0;
	while ((Count + 1) * SegmentStep < 1.f - ShooterVitalBarLayoutRules::DividerEndEpsilon)
	{
		++Count;
	}
	return Count;
}

bool FShooterVitalBarLayout::operator==(const FShooterVitalBarLayout& Other) const
{
	return DisplayD == Other.DisplayD
		&& SegmentValue == Other.SegmentValue
		&& EdgeHPSolid == Other.EdgeHPSolid
		&& EdgeHP == Other.EdgeHP
		&& EdgeShieldSolid == Other.EdgeShieldSolid
		&& EdgeContent == Other.EdgeContent
		&& EdgeMissingHP == Other.EdgeMissingHP
		&& SegmentStep == Other.SegmentStep
		&& bHealthChangeIsHeal == Other.bHealthChangeIsHeal
		&& bShieldChangeIsHeal == Other.bShieldChangeIsHeal
		&& HealthFlash == Other.HealthFlash
		&& ShieldFlash == Other.ShieldFlash;
}
