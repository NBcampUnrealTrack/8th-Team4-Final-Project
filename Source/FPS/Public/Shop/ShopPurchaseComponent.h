#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Shop/ShopTypes.h"

#include "ShopPurchaseComponent.generated.h"

class UShopComponent;
class UWeaponInventoryComponent;
class URogueAbilitySystemComponent;
class AShooterPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShopPendingStateChanged);

UCLASS(ClassGroup = (Economy))
class FPS_API UShopPurchaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UShopPurchaseComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Shop")
	void ServerPurchaseItem(FGameplayTag ItemId);

	// UI용 사전 판정. 소유/Pending 주무기는 무료 장착으로, 보호막은 기존 Pending 결제액을
	// 교체 offset으로 반영한다. 실제 거래는 ServerPurchaseItem에서 서버 기준으로 다시 검증한다.
	EShopPurchaseResult GetPurchaseEligibility(
		FGameplayTag ItemId,
		int32& OutPrice) const;

	// 모든 Pending 거래와 적용 대상을 변경 없이 확인한다.
	// GameMode는 전 참가자가 통과한 뒤에만 라운드 확정을 시작한다.
	bool CanCommitPendingPurchases() const;

	// 라운드 시작 시 Pending 거래를 확정한다. false면 해소되지 않은 Pending이 남아 있다.
	bool CommitPendingPurchases();

	// 라운드 시작 취소 시 Pending으로 남은 거래를 전부 환불한다.
	bool CancelPendingPurchases();

	/**
	 * 확정 전 상품 하나를 취소하고 환불한다.
	 * 구매/장착 입력과 환불 입력을 분리해 주무기의 무료 재장착과 의미가 충돌하지 않게 한다.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Shop")
	void ServerRefundPendingItem(FGameplayTag ItemId);

	// 현재 장착한 주무기가 이번 준비 페이즈의 Pending 구매라면 그 ItemId를 반환한다.
	// 거래 핸들은 서버에만 두고 UI에는 필요한 상태만 복제한다.
	UFUNCTION(BlueprintPure, Category = "Shop")
	FGameplayTag GetPendingWeaponItemId() const { return PendingWeaponItemId; }

	UFUNCTION(BlueprintPure, Category = "Shop")
	bool IsPendingWeaponPurchase(FGameplayTag ItemId) const
	{
		return PendingWeaponItemIds.Contains(ItemId);
	}

	UFUNCTION(BlueprintPure, Category = "Shop")
	FGameplayTag GetPendingShieldItemId() const { return PendingShieldItemId; }

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FOnShopPendingStateChanged OnPendingStateChanged;

private:
	struct FPendingWeaponCommit
	{
		FGameplayTag ItemId;
		FShopTransactionHandle Transaction;
		FGameplayTag WeaponTag;
	};

	// 주무기: 미소지 상품은 정가로 Pending 구매하고, 소지 상품은 무료 장착한다.
	bool TryPurchasePrimaryWeapon(UShopComponent& Shop, const FShopItemDefinition& Definition);
	bool TryEquipOwnedWeapon(FGameplayTag WeaponTag);

	// 권총/칼 업그레이드: 구매 즉시 확정한다.
	bool TryPurchaseWeaponUpgrade(UShopComponent& Shop, const FShopItemDefinition& Definition);

	// HP / Damage: 누적형 상품이라 구매 즉시 확정한다.
	bool TryPurchaseStatUpgrade(UShopComponent& Shop, const FShopItemDefinition& Definition);

	// 보호막: Small/Large가 하나의 슬롯을 공유한다. 거래는 Pending으로 유지하고 GE는 즉시 적용한다.
	bool TryPurchaseShield(UShopComponent& Shop, const FShopItemDefinition& Definition);

	// 교체/취소 시 확정 전 보호막 GE를 제거한다.
	void RemovePendingShieldEffect();

	// 라운드 시작 시 상품군별 Pending을 확정한다.
	bool BuildPendingWeaponCommitPlan(
		const UShopComponent& Shop,
		TArray<FPendingWeaponCommit>& OutPendingCommits,
		FGameplayTag& OutDesiredEquippedPrimary) const;
	bool CanCommitPendingShield(const UShopComponent& Shop) const;
	bool CommitPendingWeapons(
		UShopComponent& Shop,
		const TArray<FPendingWeaponCommit>& PendingCommits,
		FGameplayTag DesiredEquippedPrimary);
	bool CommitPendingShield(UShopComponent& Shop);

	// GE를 적용하고 Active 핸들을 반환한다. 적용 실패 시 무효 핸들이다.
	FActiveGameplayEffectHandle ApplyEffect(const FShopItemDefinition& Definition);

	URogueAbilitySystemComponent* GetAbilitySystem() const;

	// 교체 상품만 기존 Pending 결제액을 구매 가능 크레딧에 반영한다. 현재는 보호막만 해당한다.
	int32 GetPendingCreditOffset(const FShopItemDefinition& Definition) const;

	// 개별 Pending 환불. 주무기 런타임 장착 복원은 호출자가 처리하고, 보호막은 GE까지 제거한다.
	bool RefundPendingWeapon(UShopComponent& Shop, FGameplayTag ItemId);
	bool RefundPendingShield(UShopComponent& Shop);

	// 무료 장착은 거래를 만들지 않으므로 준비 페이즈 여부를 여기서 확인한다.
	bool IsShopPhaseOpen() const;

	AShooterPlayerState* GetPlayerState() const;
	UWeaponInventoryComponent* GetWeaponInventory() const;
	APawn* GetOwningPawn() const;

	void RefreshPendingMirror();

	UFUNCTION()
	void OnRep_PendingItems();

	// 주무기 소유권은 상품별 독립 Pending으로 보관한다. 장착 슬롯은 별도로 하나만 유지한다.
	TMap<FGameplayTag, FShopTransactionHandle> PendingWeaponTransactions;

	FShopTransactionHandle PendingShieldTransaction;

	// 확정 전 보호막 GE 핸들. Commit 시 버리고 Cancel/Replace 시 제거한다.
	FActiveGameplayEffectHandle PendingShieldEffect;

	// 현재 장착한 주무기가 Pending 구매일 때만 유효하다.
	UPROPERTY(ReplicatedUsing = OnRep_PendingItems)
	FGameplayTag PendingWeaponItemId;

	// 이번 준비 페이즈에 결제해 소유권 확정을 기다리는 주무기 목록이다.
	UPROPERTY(ReplicatedUsing = OnRep_PendingItems)
	TArray<FGameplayTag> PendingWeaponItemIds;

	UPROPERTY(ReplicatedUsing = OnRep_PendingItems)
	FGameplayTag PendingShieldItemId;

	UPROPERTY(ReplicatedUsing = OnRep_PendingItems)
	int32 PendingShieldPaidCredits = 0;
};
