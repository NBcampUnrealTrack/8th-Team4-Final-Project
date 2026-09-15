#pragma once

#include "CoreMinimal.h"
#include "ChatTypes.generated.h"

UENUM(BlueprintType)
enum class EChatChannel : uint8
{
	Team UMETA(DisplayName = "Team"),
	Match UMETA(DisplayName = "Match")
};

USTRUCT(BlueprintType)
struct FChatMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString SenderName;

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	FString MessageText;

	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	EChatChannel Channel = EChatChannel::Match;
};

// MainMenuWidgetLimits(MainMenuWidget.cpp)와 동일한 네이밍 컨벤션을 따른다.
namespace ShooterChatLimits
{
	constexpr int32 MaxMessageLength = 150;
	constexpr float MinSendIntervalSeconds = 0.5f;
}
