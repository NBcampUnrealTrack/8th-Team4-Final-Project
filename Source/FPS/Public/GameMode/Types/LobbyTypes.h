#pragma once

#include "CoreMinimal.h"
#include "LobbyTypes.generated.h"

UENUM(BlueprintType)
enum class ELobbyMatchMode : uint8
{
	Bomb UMETA(DisplayName = "Bomb"),
	Domination UMETA(DisplayName = "Domination")
};
