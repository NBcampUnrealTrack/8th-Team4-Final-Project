#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "DominationLoadoutManager.generated.h"

class ADominationPlayerState;
class APawn;
struct FStreamableHandle;
enum class EDominationCombatClass : uint8;

enum class EDominationCombatAssetStatus : uint8
{
	Ready,
	Loading,
	Failed
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDominationCombatAssetsReady, EDominationCombatClass, bool);

/** 선택한 Domination 클래스의 무기 인벤토리와 Pawn 로드아웃을 일관되게 구성한다. */
UCLASS()
class FPS_API UDominationLoadoutManager : public UObject
{
	GENERATED_BODY()

public:
	bool HasRequiredWeaponRegistryEntries() const;
	static bool GatherCombatAssetPaths(EDominationCombatClass CombatClass, TArray<FSoftObjectPath>& OutPaths);
	static bool AreCombatAssetsReady(EDominationCombatClass CombatClass);
	EDominationCombatAssetStatus EnsureCombatAssets(EDominationCombatClass CombatClass);
	void Shutdown();
	FOnDominationCombatAssetsReady OnCombatAssetsReady;

	// 최초 스폰 전에 클래스 무기를 보유·해금 상태로 확정한다.
	bool InitializeCombatClassInventory(ADominationPlayerState* PlayerState) const;

	// 재스폰에서는 기존 인벤토리를 초기화하지 않고, 선택 클래스의 주무기 상태만 보장한다.
	bool EnsureCombatClassInventory(ADominationPlayerState* PlayerState) const;

	bool ApplyCombatClassLoadout(
		const ADominationPlayerState* PlayerState,
		APawn* Pawn) const;

private:
	void HandleCombatAssetsLoaded(EDominationCombatClass CombatClass);
	static FGameplayTag ResolvePrimaryWeaponTag(EDominationCombatClass CombatClass);
	TMap<EDominationCombatClass, TSharedPtr<FStreamableHandle>> CombatAssetHandles;
	TMap<EDominationCombatClass, EDominationCombatAssetStatus> CombatAssetStates;
	TMap<EDominationCombatClass, double> CombatAssetRequestTimes;
};
