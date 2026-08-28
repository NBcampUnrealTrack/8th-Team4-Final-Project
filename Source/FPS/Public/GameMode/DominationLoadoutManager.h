#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "DominationLoadoutManager.generated.h"

class ADominationPlayerState;
class APawn;
enum class EDominationCombatClass : uint8;

/** 선택한 Domination 클래스의 무기 인벤토리와 Pawn 로드아웃을 일관되게 구성한다. */
UCLASS()
class FPS_API UDominationLoadoutManager : public UObject
{
	GENERATED_BODY()

public:
	bool HasRequiredWeaponRegistryEntries() const;

	// 최초 스폰 전에 클래스 무기를 보유·해금 상태로 확정한다.
	bool InitializeCombatClassInventory(ADominationPlayerState* PlayerState) const;

	bool ApplyCombatClassLoadout(
		const ADominationPlayerState* PlayerState,
		APawn* Pawn) const;

private:
	static FGameplayTag ResolvePrimaryWeaponTag(EDominationCombatClass CombatClass);
};
