#pragma once

#include "CoreMinimal.h"
#include "ShooterCharacterTypes.generated.h"

UENUM(BlueprintType)
enum class ETurningInPlace : uint8
{
	Left,
	Right,
	NotTurning
};

UENUM(BlueprintType)
enum class ECharacterGait : uint8
{
	Walking,
	Sprinting,
};
