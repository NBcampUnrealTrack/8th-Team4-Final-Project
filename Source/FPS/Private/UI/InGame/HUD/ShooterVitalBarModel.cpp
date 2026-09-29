// ShooterVitalBarModel.cpp
#include "UI/InGame/HUD/ShooterVitalBarModel.h"

void FShooterVitalBarModel::QueueTarget(const FShooterVitalValues& InValues)
{
	PendingValues = InValues;
	bHasPendingValues = true;
}

void FShooterVitalBarModel::SnapToTarget()
{
	if (bHasPendingValues)
	{
		float NewHealth = 0.f;
		float NewShield = 0.f;
		TakePendingValues(NewHealth, NewShield);
		Health.Real = NewHealth;
		Shield.Real = NewShield;
	}

	SnapTrack(Health, Health.Real);
	SnapTrack(Shield, Shield.Real);
}

FShooterVitalBarTickResult FShooterVitalBarModel::Tick(const float DeltaSeconds, const FShooterVitalBarSettings& Settings)
{
	const FShooterVitalBarLayout Before = GetLayout(Settings);

	// 값 적용
	if (bHasPendingValues)
	{
		ApplyPendingValues(Settings);
	}
 
	const float SafeDeltaSeconds = FMath::Max(0.f, DeltaSeconds);
	AdvanceTrack(Health, SafeDeltaSeconds, Settings);
	AdvanceTrack(Shield, SafeDeltaSeconds, Settings);

	FShooterVitalBarTickResult Result;
	Result.bVisualChanged = GetLayout(Settings) != Before;
	Result.bNeedsMoreUpdates = NeedsUpdates(Health) || NeedsUpdates(Shield);
	return Result;
}

void FShooterVitalBarModel::TakePendingValues(float& OutHealth, float& OutShield)
{
	bHasPendingValues = false;

	MaxHealth = FMath::Max(0.f, PendingValues.MaxHealth);
	MaxShield = FMath::Max(0.f, PendingValues.MaxShield);

	// 복제 순서로 잠깐 용량을 넘어도 자른다
	OutHealth = FMath::Clamp(PendingValues.Health, 0.f, MaxHealth);
	OutShield = FMath::Clamp(PendingValues.Shield, 0.f, MaxShield);
}

void FShooterVitalBarModel::ApplyPendingValues(const FShooterVitalBarSettings& Settings)
{
	float NewHealth = 0.f;
	float NewShield = 0.f;
	TakePendingValues(NewHealth, NewShield);

	// 부활 · 라운드 초기화 · 첫 값
	if (Health.Real <= 0.f && NewHealth > 0.f)
	{
		SnapTrack(Health, NewHealth);
		SnapTrack(Shield, NewShield);
		return;
	}

	ApplyToTrack(Health, NewHealth, Settings);
	ApplyToTrack(Shield, NewShield, Settings);
}

void FShooterVitalBarModel::ApplyToTrack(FShooterVitalTrack& Track, const float NewReal, const FShooterVitalBarSettings& Settings)
{

	if (NewReal < Track.Real && NewReal < Track.Trail)
	{
		Track.HoldRemaining = Settings.HoldSeconds;
		Track.Flash = 1.f;
	}
	else if (NewReal > Track.Real && NewReal > Track.Trail)
	{
		Track.HealDelayRemaining = Settings.HealDelaySeconds;
	}

	Track.Real = NewReal;
}

void FShooterVitalBarModel::AdvanceTrack(FShooterVitalTrack& Track, const float DeltaSeconds, const FShooterVitalBarSettings& Settings)
{
	if (Track.Flash > 0.f)
	{
		Track.Flash = (Settings.FlashSeconds > 0.f)
			? FMath::Max(0.f, Track.Flash - DeltaSeconds / Settings.FlashSeconds)
			: 0.f;
	}

	if (Track.Trail > Track.Real)
	{
		const float HoldUsed = FMath::Min(Track.HoldRemaining, DeltaSeconds);
		Track.HoldRemaining -= HoldUsed;

		const float DrainTime = DeltaSeconds - HoldUsed;
		if (DrainTime > 0.f)
		{
			Track.Trail = FMath::Max(Track.Real, Track.Trail - Settings.DrainPerSecond * DrainTime);
		}
	}
	else if (Track.Trail < Track.Real)
	{
		const float DelayUsed = FMath::Min(Track.HealDelayRemaining, DeltaSeconds);
		Track.HealDelayRemaining -= DelayUsed;

		const float RiseTime = DeltaSeconds - DelayUsed;
		if (RiseTime > 0.f)
		{
			Track.Trail = FMath::Min(Track.Real, Track.Trail + Settings.HealRisePerSecond * RiseTime);
		}
	}

	// 정착하면 대기 시간을 비운다
	if (Track.Trail == Track.Real)
	{
		Track.HoldRemaining = 0.f;
		Track.HealDelayRemaining = 0.f;
	}
}

void FShooterVitalBarModel::SnapTrack(FShooterVitalTrack& Track, const float Value)
{
	Track.Real = Value;
	Track.Trail = Value;
	Track.HoldRemaining = 0.f;
	Track.HealDelayRemaining = 0.f;
	Track.Flash = 0.f;
}

bool FShooterVitalBarModel::NeedsUpdates(const FShooterVitalTrack& Track)
{
	return Track.Trail != Track.Real || Track.Flash > 0.f;
}

FShooterVitalBarLayout FShooterVitalBarModel::GetLayout(const FShooterVitalBarSettings& Settings) const
{
	FShooterVitalBarLayoutInput In;
	In.MaxHealth = MaxHealth;
	In.MaxShield = MaxShield;
	In.HealthReal = Health.Real;
	In.HealthTrail = Health.Trail;
	In.ShieldReal = Shield.Real;
	In.ShieldTrail = Shield.Trail;
	In.HealthFlash = Health.Flash;
	In.ShieldFlash = Shield.Flash;
	In.SegmentValue = Settings.SegmentValue;
	return FShooterVitalBarLayout::Compute(In);
}

float FShooterVitalBarModel::GetTargetD() const
{
	return MaxHealth + MaxShield;
}

const FShooterVitalTrack& FShooterVitalBarModel::GetHealthTrack() const
{
	return Health;
}

const FShooterVitalTrack& FShooterVitalBarModel::GetShieldTrack() const
{
	return Shield;
}
