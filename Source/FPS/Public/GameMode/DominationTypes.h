#pragma once

#include "CoreMinimal.h"
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

USTRUCT(BlueprintType)
struct FPS_API FDominationMatchFlowState
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Game Flow")
	EDominationMatchPhase Phase = EDominationMatchPhase::WaitingForPlayers;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Domination|Game Flow")
	float PhaseEndServerTime = 0.0f;
};
