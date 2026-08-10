#pragma once

#include "CoreMinimal.h"
#include "DataJackTypes.generated.h"

UENUM(BlueprintType)
enum class EDataJackState : uint8
{
	Idle UMETA(DisplayName = "Idle on Ground"),
	Carried UMETA(DisplayName = "Carried by Player"),
	Planting UMETA(DisplayName = "Planting in Progress"),
	Planted UMETA(DisplayName = "Planted and Counting Down"),
	Defusing UMETA(DisplayName = "Defusing in Progress"),
	Detonated UMETA(DisplayName = "Successfully Detonated"),
	Defused UMETA(DisplayName = "Successfully Defused")
};
