// ShooterVitalBarBurst.cpp
#include "UI/InGame/HUD/ShooterVitalBarBurst.h"

namespace ShooterVitalBarBurstRules
{
	// 이어진 칸 구간
	struct FCellRun
	{
		int32 First = 0;
		int32 Last = -1;
	};

	// 값 구간이 닿은 칸
	bool FindTouchedCells(const float Offset, const float Bottom, const float Top, const float CellValue, FCellRun& OutRun)
	{
		if (Top <= Bottom)
		{
			return false;
		}

		OutRun.First = FMath::FloorToInt((Offset + Bottom) / CellValue);
		OutRun.Last = FMath::CeilToInt((Offset + Top) / CellValue) - 1;
		return OutRun.Last >= OutRun.First;
	}

	int32 CountCells(const FCellRun& Run)
	{
		return Run.Last - Run.First + 1;
	}
}

void FShooterVitalBarBurst::Emit(const FShooterVitalBarBurstInput& In, const FShooterVitalBarBurstSettings& Settings)
{
	using namespace ShooterVitalBarBurstRules;

	// 값이 오르면 이미 터진 위치도 따라 오른다
	if (In.HealthAfter > In.HealthBefore)
	{
		HealthFront = FMath::Max(HealthFront, In.HealthAfter);
	}
	if (In.ShieldAfter > In.ShieldBefore)
	{
		ShieldFront = FMath::Max(ShieldFront, In.ShieldAfter);
	}

	if (In.DisplayD <= 0.f || Settings.CellValue <= 0.f)
	{
		return;
	}

	// 용량 감소분과 이미 터진 몫은 뺀다
	const float HealthTop = FMath::Min3(In.HealthBefore, In.MaxHealth, HealthFront);
	const float ShieldTop = FMath::Min3(In.ShieldBefore, In.MaxShield, ShieldFront);

	FCellRun Runs[2];
	int32 RunCount = 0;

	FCellRun ShieldRun;
	if (FindTouchedCells(In.ShieldStart, In.ShieldAfter, ShieldTop, Settings.CellValue, ShieldRun))
	{
		ShieldFront = FMath::Max(0.f, ShieldRun.First * Settings.CellValue - In.ShieldStart);
		Runs[RunCount++] = ShieldRun;
	}

	FCellRun HealthRun;
	if (FindTouchedCells(0.f, In.HealthAfter, HealthTop, Settings.CellValue, HealthRun))
	{
		HealthFront = HealthRun.First * Settings.CellValue;
		Runs[RunCount++] = HealthRun;
	}

	if (RunCount == 0)
	{
		return;
	}

	// 오른쪽 구간부터
	if (RunCount == 2 && Runs[1].Last > Runs[0].Last)
	{
		Swap(Runs[0], Runs[1]);
	}

	// 맞닿거나 겹치면 한 구간
	if (RunCount == 2 && Runs[1].Last >= Runs[0].First - 1)
	{
		Runs[0].First = FMath::Min(Runs[0].First, Runs[1].First);
		RunCount = 1;
	}

	int32 TotalCells = 0;
	for (int32 Index = 0; Index < RunCount; ++Index)
	{
		TotalCells += CountCells(Runs[Index]);
	}

	// 큰 피해는 시간차를 압축
	const float Stagger = (TotalCells > 1)
		? FMath::Min(Settings.StaggerSeconds, Settings.MaxSpreadSeconds / static_cast<float>(TotalCells - 1))
		: 0.f;

	int32 CellsBefore = 0;
	for (int32 Index = 0; Index < RunCount; ++Index)
	{
		Push(Runs[Index].First, Runs[Index].Last, CellsBefore * Stagger, Stagger, Settings);
		CellsBefore += CountCells(Runs[Index]);
	}
}

bool FShooterVitalBarBurst::Tick(const float DeltaSeconds)
{
	const float SafeDeltaSeconds = FMath::Max(0.f, DeltaSeconds);

	bool bAnyActive = false;
	for (FShooterVitalBurstInstance& Instance : Instances)
	{
		if (!Instance.bActive)
		{
			continue;
		}

		Instance.Age += SafeDeltaSeconds;
		if (Instance.Age >= Instance.EndAge)
		{
			Instance.bActive = false;
			continue;
		}

		bAnyActive = true;
	}
	return bAnyActive;
}

void FShooterVitalBarBurst::Reset(const float HealthReal, const float ShieldReal)
{
	for (FShooterVitalBurstInstance& Instance : Instances)
	{
		Instance = FShooterVitalBurstInstance();
	}

	HealthFront = HealthReal;
	ShieldFront = ShieldReal;
}

bool FShooterVitalBarBurst::IsActive() const
{
	for (const FShooterVitalBurstInstance& Instance : Instances)
	{
		if (Instance.bActive)
		{
			return true;
		}
	}
	return false;
}

const FShooterVitalBurstInstance& FShooterVitalBarBurst::GetInstance(const int32 Index) const
{
	check(Index >= 0 && Index < MaxInstances);
	return Instances[Index];
}

void FShooterVitalBarBurst::Push(const int32 FirstCell, const int32 LastCell, const float StartDelay, const float Stagger, const FShooterVitalBarBurstSettings& Settings)
{
	// 빈 슬롯, 없으면 가장 먼저 끝날 구간
	int32 Slot = 0;
	float ShortestRemaining = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < MaxInstances; ++Index)
	{
		if (!Instances[Index].bActive)
		{
			Slot = Index;
			break;
		}

		const float Remaining = Instances[Index].EndAge - Instances[Index].Age;
		if (Remaining < ShortestRemaining)
		{
			ShortestRemaining = Remaining;
			Slot = Index;
		}
	}

	FShooterVitalBurstInstance& Instance = Instances[Slot];
	Instance.FirstCell = static_cast<float>(FirstCell);
	Instance.LastCell = static_cast<float>(LastCell);
	Instance.Age = -StartDelay;
	Instance.Stagger = Stagger;
	Instance.EndAge = (LastCell - FirstCell) * Stagger + Settings.ChunkSeconds;
	Instance.bActive = true;
}
