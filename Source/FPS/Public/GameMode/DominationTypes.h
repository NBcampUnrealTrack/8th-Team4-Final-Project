#pragma once

#include "CoreMinimal.h"
#include "GameMode/TeamTypes.h"
#include "DominationTypes.generated.h"

UENUM(BlueprintType)
enum class EDominationMatchPhase : uint8
{
	WaitingForPlayers UMETA(DisplayName = "Waiting For Players"),
	ClassSelecting UMETA(DisplayName = "Class Selecting"),
	InProgress UMETA(DisplayName = "In Progress"),
	MatchEnded UMETA(DisplayName = "Match Ended")
};

UENUM(BlueprintType)
enum class EDominationCombatClass : uint8
{
	Unselected UMETA(DisplayName = "Unselected"),
	Trooper UMETA(DisplayName = "Trooper"),
	Ranger UMETA(DisplayName = "Ranger"),
	Vanguard UMETA(DisplayName = "Vanguard")
};

UENUM(BlueprintType)
enum class EDominationPointId : uint8
{
	None UMETA(DisplayName = "None"),
	A UMETA(DisplayName = "A"),
	B UMETA(DisplayName = "B"),
	C UMETA(DisplayName = "C"),
	D UMETA(DisplayName = "D"),
	E UMETA(DisplayName = "E")
};

UENUM(BlueprintType)
enum class EDominationObjectivePhase : uint8
{
	Inactive UMETA(DisplayName = "Inactive"),
	CaptureEnabled UMETA(DisplayName = "Capture Enabled"),
	Repositioning UMETA(DisplayName = "Repositioning")
};

namespace DominationRules
{
	inline constexpr int32 PointCount = 5;
	inline constexpr int32 SpawnBoundaryCount = PointCount + 2;
	inline constexpr int32 RoundsToWin = 3;
	inline constexpr int32 MaxRounds = RoundsToWin * 2 - 1;

	FORCEINLINE int32 GetPointIndex(const EDominationPointId PointId)
	{
		switch (PointId)
		{
		case EDominationPointId::A:
			return 0;
		case EDominationPointId::B:
			return 1;
		case EDominationPointId::C:
			return 2;
		case EDominationPointId::D:
			return 3;
		case EDominationPointId::E:
			return 4;
		default:
			return INDEX_NONE;
		}
	}

	FORCEINLINE EDominationPointId GetPointId(const int32 PointIndex)
	{
		switch (PointIndex)
		{
		case 0:
			return EDominationPointId::A;
		case 1:
			return EDominationPointId::B;
		case 2:
			return EDominationPointId::C;
		case 3:
			return EDominationPointId::D;
		case 4:
			return EDominationPointId::E;
		default:
			return EDominationPointId::None;
		}
	}

	// Team A advances from A to E, while Team B advances from E to A.
	FORCEINLINE int32 GetAttackDirection(const EGameTeam Team)
	{
		switch (Team)
		{
		case EGameTeam::TeamA:
			return 1;
		case EGameTeam::TeamB:
			return -1;
		default:
			return 0;
		}
	}

	FORCEINLINE int32 GetSpawnBoundaryIndex(
		const EGameTeam Team,
		const int32 ActivePointIndex)
	{
		const int32 AttackDirection = GetAttackDirection(Team);
		if (ActivePointIndex < 0 || ActivePointIndex >= PointCount || AttackDirection == 0)
		{
			return INDEX_NONE;
		}

		// Team A uses the A-side boundary; Team B uses the E-side boundary.
		return ActivePointIndex + (AttackDirection < 0 ? 2 : 0);
	}
}

USTRUCT(BlueprintType)
struct FPS_API FDominationCaptureRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Objective", meta = (ClampMin = "0.1", Units = "s"))
	float OnePlayerCaptureTime = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Objective", meta = (ClampMin = "0.1", Units = "s"))
	float TwoPlayerCaptureTime = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Objective", meta = (ClampMin = "0.1", Units = "s"))
	float ThreeOrMorePlayerCaptureTime = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Objective", meta = (ClampMin = "0.1", Units = "s"))
	float SuddenDeathDelay = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Objective", meta = (ClampMin = "0.1", Units = "s"))
	float SuddenDeathCaptureTime = 10.0f;

	float GetCaptureTime(const int32 PlayerCount, const bool bSuddenDeath) const
	{
		if (bSuddenDeath)
		{
			return SuddenDeathCaptureTime;
		}

		if (PlayerCount >= 3)
		{
			return ThreeOrMorePlayerCaptureTime;
		}

		return PlayerCount == 2 ? TwoPlayerCaptureTime : OnePlayerCaptureTime;
	}
};

USTRUCT(BlueprintType)
struct FPS_API FDominationCaptureState
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Objective")
	EGameTeam CapturingTeam = EGameTeam::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Objective", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CaptureProgress = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Objective")
	int32 TeamAPlayerCount = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Objective")
	int32 TeamBPlayerCount = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Objective")
	bool bContested = false;
};

USTRUCT(BlueprintType)
struct FPS_API FDominationMatchFlowState
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Game Flow")
	EDominationMatchPhase Phase = EDominationMatchPhase::WaitingForPlayers;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Game Flow")
	float PhaseEndServerTime = 0.0f;
};

USTRUCT(BlueprintType)
struct FPS_API FDominationObjectiveFlowState
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Objective")
	EDominationPointId ActivePointId = EDominationPointId::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Objective")
	EDominationObjectivePhase Phase = EDominationObjectivePhase::Inactive;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Objective")
	float PhaseEndServerTime = 0.0f;
};
