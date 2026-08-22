#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Weapon/WeaponTypes.h"

#include "WeaponInventoryComponent.generated.h"

class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponInventoryChanged);

USTRUCT()
struct FWeaponOwnershipState
{
	GENERATED_BODY()

	UPROPERTY()
	EWeaponType WeaponType = EWeaponType::None;

	// 인벤토리에 보유해 전투에서 쓸 수 있는가.
	UPROPERTY()
	bool bPossessed = false;

	// 전용 증강 카드 풀이 열렸는가.
	UPROPERTY()
	bool bUnlocked = false;
};

/**
 * PlayerState가 소유하는 무기 영구 상태. 폰 수명과 무관하며 7R 공수 전환에도 유지된다.
 *
 * Pending(장바구니)은 여기 없다. 미확정 결제는 ShopPurchaseComponent의 거래 원장이 단독으로
 * 들고 있고, 이 컴포넌트에는 라운드 시작 시점에 확정된 값만 들어온다.
 *
 * 폰, CombatComponent, Shop 원장을 알지 못한다.
 */
UCLASS(ClassGroup = (Rogue))
class FPS_API UWeaponInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 아바타 초기화 직후 소유자가 호출한다. 해금 태그를 ASC에 얹기 위해 필요하다.
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

	// --- 조회 ---

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|Inventory")
	bool IsPossessed(EWeaponType WeaponType) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|Inventory")
	bool IsUnlocked(EWeaponType WeaponType) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|Inventory")
	EWeaponType GetEquippedPrimary() const { return EquippedPrimary; }

	// --- 서버 변경. 게임 흐름 순서대로 ---

	// 1. 경기 시작. 권총/칼만 소지, 둘 다 미해금, 주무기 없음.
	void ServerInitializeForNewMatch();

	// 4. 라운드 시작 Commit. 소지 + 해금 + 장착을 한 번에 확정한다.
	//    셋을 쪼개면 중간 실패 시 결제만 닫히고 상태가 어긋난다.
	bool ServerConfirmPrimaryPurchase(EWeaponType WeaponType);

	// 6. 이미 소지한 주무기 사이의 무료 장착 전환.
	bool ServerSetEquippedPrimary(EWeaponType WeaponType);

	// 권총/칼 업그레이드 확정. 소지는 이미 true이므로 해금만 연다.
	bool ServerConfirmWeaponUpgrade(EWeaponType WeaponType);

	// 상점에서 하나의 슬롯을 두고 경쟁하는 무기인가.
	static bool IsPrimaryWeapon(EWeaponType WeaponType);

	UPROPERTY(BlueprintAssignable, Category = "FPS|Weapon|Inventory")
	FOnWeaponInventoryChanged OnWeaponInventoryChanged;

protected:
	virtual void BeginPlay() override;

private:
	bool HasServerAuthority() const;

	FWeaponOwnershipState* FindState(EWeaponType WeaponType);
	const FWeaponOwnershipState* FindState(EWeaponType WeaponType) const;

	void ResetToDefaults();
	void NotifyInventoryChanged();

	// 증강 드래프트는 ASC의 Unlock.Weapon.* 루즈 태그를 읽어 카드 풀을 정한다(문서 6장).
	// 확정 상태가 바뀔 때마다 그 태그를 맞춰 준다.
	void SyncUnlockTags();

	UFUNCTION()
	void OnRep_Inventory();

	UPROPERTY(ReplicatedUsing = OnRep_Inventory)
	TArray<FWeaponOwnershipState> WeaponStates;

	UPROPERTY(ReplicatedUsing = OnRep_Inventory)
	EWeaponType EquippedPrimary = EWeaponType::None;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
};
