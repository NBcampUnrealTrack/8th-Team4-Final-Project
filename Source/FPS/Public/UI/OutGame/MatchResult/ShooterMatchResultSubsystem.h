// ShooterMatchResultSubSystem
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/OutGame/MatchResult/ShooterMatchResultTypes.h"
#include "ShooterMatchResultSubsystem.generated.h"

class UShooterMatchResultScreen;
class ABaseTeamMatchGameState;

/**
 * 매치 결과 보관하고 로비 도착시 결과 화면 띄움
 */
UCLASS()
class FPS_API UShooterMatchResultSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	// 매치 종료 시점의 결과를 보존한다.
	// 로컬팀이 없거나 승자가 없으면 아무것도 하지않음
	void CaptureSnapshot(const ABaseTeamMatchGameState* MatchGameState);
	
	const FShooterMatchResultSnapshot& GetSnapshot() const;
	
private:
	// 최종 목적지 월드 도착. 전환 맵에서는 오지 않음
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);
	
	// GameState로 로비/라운드/그 외를 가른다
	void EvaluateWorld();
	void ClearEvaluateTimer();
	
	// 위젯 만들어 띄운다
	bool PresentResultWidget(APlayerController* OwningPlayer);
	
	void RemoveResultWidget();
	
	void HandleCloseRequested();
	
	// 위젯이 있던 World가 사라지면 위젯 정리 Snapshot은 건들지않음
	void HandleWorldBeginTearDown(UWorld* TearingDownWorld);
	
	UPROPERTY(Transient)
	TObjectPtr<UShooterMatchResultScreen> ActiveWidget;
	
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle WorldTearDownHandle;
	
	// 판정 대상 월드 재시도 중에 다른 월드로 바뀌면 루프를 버림
	TWeakObjectPtr<UWorld> PendingWorld;
	TWeakObjectPtr<UWorld> WidgetWorld;
	
	FTimerHandle EvaluateTimerHandle;
	int32 EvaluateAttemptsLeft = 0;
	
	// GameState 액터 복제 도착 기다리는 횟수
	static constexpr int32 MaxEvaluateAttempts = 50;
	static constexpr float EvaluateInterval = 0.1f;
	
	FShooterMatchResultSnapshot Snapshot;
};
