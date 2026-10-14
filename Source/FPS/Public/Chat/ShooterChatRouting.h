// ShooterChatRouting.h
#pragma once

#include "CoreMinimal.h"
#include "Chat/ChatTypes.h"

class AGameStateBase;
class APlayerState;
class AShooterPlayerController;

// 채팅/핑 수신자 라우팅. 인게임(ABaseTeamGameState)과 대기방(ALobbyGameState)이 서로 다른
// GameState 계열이라 특정 GameState 멤버로 두지 않고 AGameStateBase::PlayerArray만 보고 동작한다.
// 팀 판정은 ABaseTeamPlayerState::GetTeam() 기준 - 대기방에서 팀을 안 고른 플레이어끼리는
// EGameTeam::None 팀으로 묶인다.
namespace ShooterChatRouting
{
	// 서버 전용. Channel==Match면 PlayerArray 전원, Channel==Team이면 Sender와 같은 팀의
	// 컨트롤러에 대해서만 Visitor를 호출한다. 적팀 컨트롤러는 아예 호출되지 않으므로
	// 텍스트 채팅 데이터 자체가 적팀 클라이언트로 전송되지 않는다.
	void ForEachChatRecipient(
		const AGameStateBase* GameState,
		const APlayerState* Sender,
		EChatChannel Channel,
		TFunctionRef<void(AShooterPlayerController*)> Visitor);

	// 서버 전용. 귓속말(/w) 대상을 플레이어 이름(대소문자 무시, 완전 일치)으로 찾는다.
	// 못 찾으면 nullptr.
	AShooterPlayerController* FindChatRecipientByName(const AGameStateBase* GameState, const FString& PlayerName);

	// 클라이언트에서도 호출 가능(PlayerArray는 복제됨). 귓속말 자동완성 후보용으로
	// 이름이 Prefix로 시작하는 플레이어 이름을 전부 반환한다(PlayerArray 크기만큼이 자연스러운
	// 상한이라 별도 개수 제한은 없다 - Box_Suggestions 쪽 스크롤이 넘치는 걸 처리한다).
	// ExcludeName과 일치하는 이름(보통 본인)은 후보에서 뺀다.
	TArray<FString> GetPlayerNamesMatchingPrefix(
		const AGameStateBase* GameState, const FString& Prefix, const FString& ExcludeName = FString());
}
