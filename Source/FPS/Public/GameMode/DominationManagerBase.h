#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DominationManagerBase.generated.h"

class ADominationGameMode;
class ADominationGameState;
class FTimerManager;

/**
 * Domination 전용 상태형 매니저가 공유하는 실행 컨텍스트와 수명주기 계약이다.
 * 소유 GameMode, World, GameState, TimerManager 접근 규칙을 한 곳에서 유지한다.
 */
UCLASS(Abstract)
class FPS_API UDominationManagerBase : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;
	virtual void Shutdown();

protected:
	bool InitializeManager(ADominationGameMode* InOwnerGameMode);
	ADominationGameMode* GetOwnerGameMode() const;
	ADominationGameState* GetDominationGameState() const;
	FTimerManager* GetTimerManager() const;

private:
	TWeakObjectPtr<ADominationGameMode> OwnerGameMode;
};
