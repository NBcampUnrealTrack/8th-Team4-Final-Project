#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DominationPreloadSubsystem.generated.h"

class ULoadingScreenSubsystem;

/** 이동 진행 상태만 연다. 전투 클래스와 표시 자산은 사용 지점에서 비동기로 준비한다. */
UCLASS()
class FPS_API UDominationPreloadSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void EnsurePreloadStarted(const UWorld* ContextWorld);

private:
	void HandleSeamlessTravelStart(UWorld* DepartingWorld, const FString& DestinationMap);
	void HandleSeamlessTravelTransition(UWorld* World);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);
	bool IsOwnedByThisGameInstance(const UWorld* World) const;
	ULoadingScreenSubsystem* GetLoadingScreenSubsystem() const;

	FDelegateHandle SeamlessTravelHandle;
	FDelegateHandle SeamlessTransitionHandle;
	FDelegateHandle PostLoadMapHandle;
	bool bTravelProgressStarted = false;
	double TravelStartSeconds = 0.0;
};
