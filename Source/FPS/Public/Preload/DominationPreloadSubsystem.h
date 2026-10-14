#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "DominationPreloadSubsystem.generated.h"

class ULoadingScreenSubsystem;
struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCombatPreloadFinished, bool);

// 전투 자산을 이동 전에 읽고 도착한 월드에서도 유지한다.
UCLASS()
class FPS_API UDominationPreloadSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void PrepareForMap(const FString& DestinationMap);
	bool IsPreloadReady() const { return bPreloadReady; }
	bool HasPreloadFailed() const { return bPreloadFailed; }
	void ReleasePreload();

	// 같은 목적지의 완료된 요청은 다시 시작하지 않는다.
	void EnsurePreloadStarted(const UWorld* ContextWorld);
	FOnCombatPreloadFinished OnPreloadFinished;

private:
	enum class EPreloadStage : uint8
	{
		CombatClasses,
		AppearanceCatalogs,
		AppearanceDependencies
	};

	void HandleSeamlessTravelStart(UWorld* DepartingWorld, const FString& DestinationMap);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);
	void RequestAssetStage(TArray<FSoftObjectPath> AssetPaths, uint32 Generation, EPreloadStage Stage);
	void HandleAssetStageLoaded(uint32 Generation, EPreloadStage Stage);
	void FinishPreload(bool bSuccess, uint32 Generation);
	bool IsPreparedMap(const FString& MapNameOrPath) const;
	bool IsOwnedByThisGameInstance(const UWorld* World) const;
	ULoadingScreenSubsystem* GetLoadingScreenSubsystem() const;

	FDelegateHandle SeamlessTravelHandle;
	FDelegateHandle PostLoadMapHandle;
	TArray<TSharedPtr<FStreamableHandle>> AssetPreloadHandles;
	TArray<FSoftObjectPath> RequestedAssetPaths;
	TArray<FSoftObjectPath> AppearanceConfigPaths;
	FString PreparedDestination;
	uint32 RequestGeneration = 0;
	bool bRequestStarted = false;
	bool bPreloadReady = false;
	bool bPreloadFailed = false;
	bool bIncludeVisuals = true;
	double PreloadStartTime = 0.0;
};
