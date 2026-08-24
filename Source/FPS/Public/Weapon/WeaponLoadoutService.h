#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AShooterPlayerState;
class APawn;

/**
 * PlayerState의 영구 상태를 폰의 런타임 인스턴스로 내려보내는 유일한 경로다.
 *
 *   PlayerState WeaponInventory -> WeaponLoadoutService -> Pawn CombatComponent
 *   WeaponLoadoutService -> PlayerState ASC
 *
 * CombatComponent가 PlayerState를 조회하지 않게, ShopPurchaseComponent가 무기 레지스트리와
 * 폰 순회를 알지 않게 하려고 이 한 겹을 둔다. 교체 성공 후 실제 장착 무기의 상태 태그를
 * ASC에 동기화하는 Shop-Weapon-GAS 어댑터이며 서버에서만 호출한다.
 */
namespace RogueWeaponLoadout
{
	// 주무기 슬롯을 지정한 무기로 교체하고, 실제 장착 결과를 ASC에 동기화한다.
	// 크레딧과 소지 상태는 건드리지 않는다.
	// 유효하지 않은 태그면 주무기를 비운다.
	FPS_API bool ApplyPrimaryWeapon(APawn* Pawn, FGameplayTag WeaponTag);

	// 확정된 EquippedPrimaryTag를 폰에 적용한다.
	// 폰은 라운드마다 새로 만들어지므로 이 호출이 없으면 소지한 주무기가 손에서 사라진다.
	FPS_API bool RestoreConfirmedPrimary(AShooterPlayerState* PlayerState, APawn* Pawn);
}
