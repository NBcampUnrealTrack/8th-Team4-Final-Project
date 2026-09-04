#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/TeamTypes.h"
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

// 인디케이터 색상은 절대 팀(A/B)이 아니라 로컬 플레이어 기준 아군/적으로 수정
UENUM(BlueprintType)
enum class EDominationCaptureRelation : uint8
{
	Neutral UMETA(DisplayName = "Neutral"),
	Ally UMETA(DisplayName = "Ally"),
	Enemy UMETA(DisplayName = "Enemy")
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

	// 인디케이터 색상을 모든 위젯이 동일하게 판단하도록 공용 헬퍼로 뽑음
	FORCEINLINE EDominationCaptureRelation GetCaptureRelation(
		const EGameTeam CapturingTeam,
		const EGameTeam LocalTeam)
	{
		if (CapturingTeam == EGameTeam::None || LocalTeam == EGameTeam::None)
		{
			return EDominationCaptureRelation::Neutral;
		}

		return CapturingTeam == LocalTeam ? EDominationCaptureRelation::Ally : EDominationCaptureRelation::Enemy;
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

	// 현재 Repositioning이 시작될 때 정해진 총 전환 시간이다.
	// UI는 남은 시간을 매초 계산하지 않고 이 규칙 값을 안내 문구에 사용할 수 있다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Objective", meta = (Units = "s"))
	float TransitionDuration = 0.0f;

	// 가장 최근에 완료된 점령의 도메인 정보다.
	// 일시적인 UI RPC 대신 GameState에 남겨 클라이언트가 복제 상태만으로 피드백을 구성한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Objective")
	EDominationPointId LastCapturedPointId = EDominationPointId::None;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Objective")
	EGameTeam LastCapturingTeam = EGameTeam::None;

	// 같은 거점/같은 팀이 연속으로 점령하더라도 새 사건임을 식별한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Objective")
	int32 CaptureSerial = 0;
};
