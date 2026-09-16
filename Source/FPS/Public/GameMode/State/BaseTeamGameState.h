#pragma once

#include "CoreMinimal.h"
#include "Chat/ChatTypes.h"
#include "GameMode/Types/TeamTypes.h"
#include "GameFramework/GameState.h"
#include "GameplayTagContainer.h"
#include "Templates/Function.h"
#include "BaseTeamGameState.generated.h"

class AShooterPlayerController;
class AShooterPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTeamScoreChangedSignature, EGameTeam, Team, int32, NewScore);

// 킬로그에 필요한 최소 정보
USTRUCT(BlueprintType)
struct FShooterEliminationEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Elimination")
	TObjectPtr<AShooterPlayerState> KillerPlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Elimination")
	TObjectPtr<AShooterPlayerState> VictimPlayerState = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Elimination")
	FGameplayTag WeaponTag;

	UPROPERTY(BlueprintReadOnly, Category = "Game Flow|Elimination")
	bool bHeadshot = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerEliminatedSignature, const FShooterEliminationEvent&, EliminationEvent);

// Stores mode-agnostic team scores and exposes them to gameplay UI.
UCLASS()
class FPS_API ABaseTeamGameState : public AGameState
{
	GENERATED_BODY()

public:
	ABaseTeamGameState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Score")
	int32 GetTeamScore(EGameTeam Team) const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Score")
	int32 GetTeamAScore() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Score")
	int32 GetTeamBScore() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Score")
	EGameTeam GetLeadingTeam() const;

	void AddTeamScore(EGameTeam Team, int32 ScoreAmount = 1);
	void ResetTeamScores();

	// 서버 전용. 모든 머신의 OnPlayerEliminated로 이어진다.
	void BroadcastPlayerEliminated(const FShooterEliminationEvent& EliminationEvent);

	// 서버 전용. Channel==Match면 PlayerArray 전원, Channel==Team이면 Sender와 같은 팀의
	// 컨트롤러에 대해서만 Visitor를 호출한다. 적팀 컨트롤러는 아예 호출되지 않으므로
	// 텍스트 채팅 데이터 자체가 적팀 클라이언트로 전송되지 않는다.
	void ForEachChatRecipient(
		const APlayerState* Sender, EChatChannel Channel, TFunctionRef<void(AShooterPlayerController*)> Visitor) const;

	// 서버 전용. 귓속말(/w) 대상을 플레이어 이름(대소문자 무시, 완전 일치)으로 찾는다.
	// 못 찾으면 nullptr.
	AShooterPlayerController* FindChatRecipientByName(const FString& PlayerName) const;

	// 클라이언트에서도 호출 가능(PlayerArray는 복제됨). 귓속말 자동완성 후보용으로
	// 이름이 Prefix로 시작하는 플레이어 이름을 전부 반환한다(PlayerArray 크기만큼이 자연스러운
	// 상한이라 별도 개수 제한은 없다 - Box_Suggestions 쪽 스크롤이 넘치는 걸 처리한다).
	// ExcludeName과 일치하는 이름(보통 본인)은 후보에서 뺀다.
	TArray<FString> GetPlayerNamesMatchingPrefix(
		const FString& Prefix, const FString& ExcludeName = FString()) const;

protected:
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayerEliminated(const FShooterEliminationEvent& EliminationEvent);

	UFUNCTION()
	void OnRep_TeamAScore();

	UFUNCTION()
	void OnRep_TeamBScore();

private:
	UPROPERTY(ReplicatedUsing = OnRep_TeamAScore, VisibleInstanceOnly, Category = "Game Flow|Score")
	int32 TeamAScore = 0;

	UPROPERTY(ReplicatedUsing = OnRep_TeamBScore, VisibleInstanceOnly, Category = "Game Flow|Score")
	int32 TeamBScore = 0;

public:
	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Score")
	FOnTeamScoreChangedSignature OnTeamScoreChanged;

	// 모든 머신에서 발신, 킬로그가 구독
	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Elimination")
	FOnPlayerEliminatedSignature OnPlayerEliminated;
};
