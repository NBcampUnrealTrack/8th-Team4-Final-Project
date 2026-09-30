#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DominationPreloadSubsystem.generated.h"

class ULoadingScreenSubsystem;
struct FStreamableHandle;

/**
 * Domination 전투 클래스 Pawn/UI/선택 음성 자산을 맵 이동 시작 시 비동기로 미리 읽는다.
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

	ULoadingScreenSubsystem* GetLoadingScreenSubsystem() const;

	FDelegateHandle SeamlessTravelHandle;
	FDelegateHandle PostLoadMapHandle;

	// Pawn/UI 자산을 매치 중에도 강한 스트리밍 참조로 유지한다
	TSharedPtr<FStreamableHandle> AssetPreloadHandle;

	double PreloadStartTime = 0.0;
};
