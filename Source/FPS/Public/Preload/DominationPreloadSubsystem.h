#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DominationPreloadSubsystem.generated.h"

struct FStreamableHandle;

/**
 * Domination 전투 클래스 Pawn 을 맵 이동이 시작되는 시점에 미리 읽는다.
 * PlayerController 는 맵 로딩이 끝나야 생기므로 소유자를 GameInstance 계층에 둔다.
 */
UCLASS()
class FPS_API UDominationPreloadSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 멱등. 이미 요청이 살아 있으면 아무것도 하지 않는다
	void EnsurePreloadStarted(const UWorld* ContextWorld);

private:
	void HandleSeamlessTravelStart(UWorld* DepartingWorld, const FString& DestinationMap);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);
	void ReleasePreload();

	bool IsOwnedByThisGameInstance(const UWorld* World) const;

	FDelegateHandle SeamlessTravelHandle;
	FDelegateHandle PostLoadMapHandle;

	// Pawn 스폰까지 살아 있어야 해서 매치 중에는 취소하지 않는다
	TSharedPtr<FStreamableHandle> PawnPreloadHandle;

	double PreloadStartTime = 0.0;
};
