#pragma once

#include "CoreMinimal.h"
#include "ChatTypes.generated.h"

UENUM(BlueprintType)
enum class EChatChannel : uint8
{
	Team UMETA(DisplayName = "Team"),
	Match UMETA(DisplayName = "Match"),
	// "/w 닉네임 메시지"로 진입한다. 보내거나 받으면 최근 귓속말 대상 큐(최대 2명)에 등록되어
	// 이후 Tab 채널 토글에도 포함된다.
	Whisper UMETA(DisplayName = "Whisper")
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

	// 귓속말 전용. true면 내가 보낸 귓속말이 내 로그에 에코된 것 - SenderName에는 실제 발신자(나)
	// 대신 "-> 상대이름" 표시용 문자열이 들어있다. 수신 클라이언트의 최근 귓말 큐 갱신은 이
	// 플래그가 false인 메시지(진짜로 남이 나한테 보낸 것)에 대해서만 일어난다.
	UPROPERTY(BlueprintReadOnly, Category = "Chat")
	bool bIsOutgoingEcho = false;
};

// MainMenuWidgetLimits(MainMenuWidget.cpp)와 동일한 네이밍 컨벤션을 따른다.
namespace ShooterChatLimits
{
	constexpr int32 MaxMessageLength = 150;
	constexpr float MinSendIntervalSeconds = 0.5f;

	// 귓속말 명령 접두사. "/w "가 기본이고 "/ㅈ "는 한/영 전환을 깜빡하고 두벌식 상태에서
	// w를 친 경우(같은 키 위치)를 그대로 인식해주기 위한 것. UShooterChatInputBox(자동완성
	// 트리거 감지)와 AShooterPlayerController::TryParseWhisperCommand(실제 파싱)가 같이 쓴다.
	inline bool StripWhisperCommandPrefix(const FString& Text, FString& OutRest)
	{
		static const TCHAR* Prefixes[] = { TEXT("/w "), TEXT("/ㅈ ") };
		for (const TCHAR* Prefix : Prefixes)
		{
			if (Text.StartsWith(Prefix))
			{
				OutRest = Text.RightChop(FCString::Strlen(Prefix));
				return true;
			}
		}
		return false;
	}
}
