#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DominationLoadoutManager.generated.h"

class ADominationPlayerState;
class APawn;

/**
 * 선택한 Domination 클래스의 전투 로드아웃을 Pawn에 적용한다.
 * 상점용 WeaponInventory 상태는 변경하지 않으며, 증강 쪽은 CombatClass를 기준으로 연결한다.
 */
UCLASS()
class FPS_API UDominationLoadoutManager : public UObject
{
	GENERATED_BODY()

public:
	bool ApplyCombatClassLoadout(
		const ADominationPlayerState* PlayerState,
		APawn* Pawn) const;
};
