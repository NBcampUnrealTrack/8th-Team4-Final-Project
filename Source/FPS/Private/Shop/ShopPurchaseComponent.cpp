#include "Shop/ShopPurchaseComponent.h"

#include "GAS/RogueAbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "Player/ShooterPlayerState.h"
#include "Shop/ShopComponent.h"
#include "Weapon/WeaponInventoryComponent.h"
#include "Weapon/WeaponLoadoutService.h"

DEFINE_LOG_CATEGORY_STATIC(LogShopPurchase, Log, All);

UShopPurchaseComponent::UShopPurchaseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UShopPurchaseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UShopPurchaseComponent, PendingWeaponItemId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UShopPurchaseComponent, PendingWeaponItemIds, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UShopPurchaseComponent, PendingShieldItemId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UShopPurchaseComponent, PendingShieldPaidCredits, COND_OwnerOnly);
}

AShooterPlayerState* UShopPurchaseComponent::GetPlayerState() const
{
	return Cast<AShooterPlayerState>(GetOwner());
}

UWeaponInventoryComponent* UShopPurchaseComponent::GetWeaponInventory() const
{
	const AShooterPlayerState* PlayerState = GetPlayerState();
	return IsValid(PlayerState) ? PlayerState->GetWeaponInventory() : nullptr;
}

APawn* UShopPurchaseComponent::GetOwningPawn() const
{
	const AShooterPlayerState* PlayerState = GetPlayerState();
	return IsValid(PlayerState) ? PlayerState->GetPawn() : nullptr;
}

bool UShopPurchaseComponent::IsShopPhaseOpen() const
{
	const AShooterPlayerState* PlayerState = GetPlayerState();
	const UShopComponent* Shop = IsValid(PlayerState) ? PlayerState->GetShopComponent() : nullptr;

	return IsValid(Shop) && Shop->IsPurchaseWindowOpen();
}

void UShopPurchaseComponent::RefreshPendingMirror()
{
	PendingWeaponItemIds.Reset(PendingWeaponTransactions.Num());
	for (const TPair<FGameplayTag, FShopTransactionHandle>& Pair : PendingWeaponTransactions)
	{
		if (Pair.Key.IsValid() && Pair.Value.IsValid())
		{
			PendingWeaponItemIds.Add(Pair.Key);
		}
	}

	// TMap 순회 순서와 무관하게 동일한 Pending 목록을 복제한다.
	PendingWeaponItemIds.Sort(
		[](const FGameplayTag& Left, const FGameplayTag& Right)
		{
			return Left.ToString().Compare(Right.ToString()) < 0;
		});

	if (!PendingWeaponItemId.IsValid()
		|| !PendingWeaponTransactions.Contains(PendingWeaponItemId))
	{
		PendingWeaponItemId = FGameplayTag();
	}

	PendingShieldItemId = PendingShieldTransaction.IsValid() ? PendingShieldTransaction.ItemId : FGameplayTag();
	PendingShieldPaidCredits = PendingShieldTransaction.IsValid() ? PendingShieldTransaction.PaidCredits : 0;

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}

	OnPendingStateChanged.Broadcast();
}

void UShopPurchaseComponent::OnRep_PendingItems()
{
	OnPendingStateChanged.Broadcast();
}

int32 UShopPurchaseComponent::GetPendingCreditOffset(const FShopItemDefinition& Definition) const
{
	// 기존 Pending 결제액을 재사용하는 교체 정책은 보호막에만 적용한다.
	if (Definition.ItemType == EShopItemType::Shield)
	{
		return PendingShieldItemId.IsValid() ? PendingShieldPaidCredits : 0;
	}

	return 0;
}

EShopPurchaseResult UShopPurchaseComponent::GetPurchaseEligibility(
	const FGameplayTag ItemId,
	int32& OutPrice) const
{
	OutPrice = 0;

	const AShooterPlayerState* PlayerState = GetPlayerState();
	const UShopComponent* Shop = IsValid(PlayerState) ? PlayerState->GetShopComponent() : nullptr;

	if (!IsValid(Shop))
	{
		return EShopPurchaseResult::InvalidCatalog;
	}

	FShopItemDefinition Definition;
	if (!Shop->FindItemDefinition(ItemId, Definition))
	{
		return EShopPurchaseResult::InvalidItem;
	}

	const UWeaponInventoryComponent* Inventory = GetWeaponInventory();

	// 소유했거나 이미 결제한 주무기는 추가 구매 없이 장착만 가능하다.
	if (Definition.ItemType == EShopItemType::Weapon
		&& ((IsValid(Inventory) && Inventory->IsPossessed(Definition.GrantedWeaponTag))
			|| PendingWeaponItemIds.Contains(Definition.ItemId)))
	{
		return Shop->IsPurchaseWindowOpen()
			? EShopPurchaseResult::Success
			: EShopPurchaseResult::ShopClosed;
	}

	// 이미 해금한 권총/칼 업그레이드는 재구매할 수 없다.
	if (Definition.ItemType == EShopItemType::WeaponUpgrade
		&& IsValid(Inventory)
		&& Inventory->IsUnlocked(Definition.GrantedWeaponTag))
	{
		return EShopPurchaseResult::PurchaseLimitReached;
	}

	// 가격/잔액 판정은 ShopComponent에 위임하고 보호막 교체 offset만 전달한다.
	return Shop->GetPurchaseEligibility(ItemId, OutPrice, GetPendingCreditOffset(Definition));
}

void UShopPurchaseComponent::ServerPurchaseItem_Implementation(const FGameplayTag ItemId)
{
	AShooterPlayerState* PlayerState = GetPlayerState();
	UShopComponent* Shop = IsValid(PlayerState) ? PlayerState->GetShopComponent() : nullptr;

	if (!IsValid(Shop))
	{
		return;
	}

	FShopItemDefinition Definition;
	if (!Shop->FindItemDefinition(ItemId, Definition))
	{
		return;
	}

	switch (Definition.ItemType)
	{
	case EShopItemType::Weapon:
		TryPurchasePrimaryWeapon(*Shop, Definition);
		break;

	case EShopItemType::WeaponUpgrade:
		TryPurchaseWeaponUpgrade(*Shop, Definition);
		break;

	case EShopItemType::HealthUpgrade:
	case EShopItemType::DamageUpgrade:
		TryPurchaseStatUpgrade(*Shop, Definition);
		break;

	case EShopItemType::Shield:
		TryPurchaseShield(*Shop, Definition);
		break;

	default:
		break;
	}
}

// --------------------------------------------------
// 주무기 구매 / 장착

bool UShopPurchaseComponent::TryPurchasePrimaryWeapon(
	UShopComponent& Shop,
	const FShopItemDefinition& Definition)
{
	UWeaponInventoryComponent* Inventory = GetWeaponInventory();
	const FGameplayTag WeaponTag = Definition.GrantedWeaponTag;

	if (!IsValid(Inventory) || !UWeaponInventoryComponent::IsPrimaryWeaponTag(WeaponTag))
	{
		return false;
	}

	// 확정 소유한 무기는 무료 장착한다.
	if (Inventory->IsPossessed(WeaponTag))
	{
		return TryEquipOwnedWeapon(WeaponTag);
	}

	// 같은 준비 페이즈에서 이미 결제한 무기는 재결제 없이 다시 장착한다.
	if (PendingWeaponTransactions.Contains(Definition.ItemId))
	{
		if (!IsShopPhaseOpen())
		{
			return false;
		}

		if (!RogueWeaponLoadout::ApplyPrimaryWeapon(GetOwningPawn(), WeaponTag))
		{
			return false;
		}

		PendingWeaponItemId = Definition.ItemId;
		RefreshPendingMirror();
		return true;
	}

	// 미소지 주무기는 다른 Pending과 독립적으로 정가 결제한다.
	const FShopTransactionResult Result = Shop.TryBeginTransaction(Definition.ItemId);
	if (!Result.WasSuccessful())
	{
		return false;
	}

	PendingWeaponTransactions.Add(Definition.ItemId, Result.Transaction);

	// 장착 실패 시 방금 생성한 거래만 되돌린다.
	if (!RogueWeaponLoadout::ApplyPrimaryWeapon(GetOwningPawn(), WeaponTag))
	{
		if (Shop.RefundTransaction(Result.Transaction) == EShopPurchaseResult::Success)
		{
			PendingWeaponTransactions.Remove(Definition.ItemId);
		}

		RefreshPendingMirror();
		return false;
	}

	PendingWeaponItemId = Definition.ItemId;
	RefreshPendingMirror();

	UE_LOG(LogShopPurchase, Log,
		TEXT("Weapon purchase pending: Player=%s Weapon=%s Price=%d Credits=%d->%d"),
		*GetNameSafe(GetOwner()),
		*WeaponTag.ToString(),
		Result.Transaction.PaidCredits,
		Result.CreditsBefore,
		Result.CreditsAfter);

	return true;
}

// --------------------------------------------------
// 소유 주무기 장착

bool UShopPurchaseComponent::TryEquipOwnedWeapon(const FGameplayTag WeaponTag)
{
	// 무료 장착은 거래를 만들지 않으므로 페이즈를 직접 확인한다.
	if (!IsShopPhaseOpen())
	{
		return false;
	}

	UWeaponInventoryComponent* Inventory = GetWeaponInventory();
	if (!IsValid(Inventory) || !Inventory->IsPossessed(WeaponTag))
	{
		return false;
	}

	const FGameplayTag PreviousConfirmedPrimary = Inventory->GetEquippedPrimaryTag();

	if (!RogueWeaponLoadout::ApplyPrimaryWeapon(GetOwningPawn(), WeaponTag))
	{
		return false;
	}

	// 확정 소유 무기의 장착 선택은 즉시 Inventory에 반영한다.
	if (!Inventory->ServerSetEquippedPrimary(WeaponTag))
	{
		RogueWeaponLoadout::ApplyPrimaryWeapon(GetOwningPawn(), PreviousConfirmedPrimary);
		return false;
	}

	PendingWeaponItemId = FGameplayTag();
	RefreshPendingMirror();
	return true;
}

// --------------------------------------------------
// 권총 / 칼 업그레이드

bool UShopPurchaseComponent::TryPurchaseWeaponUpgrade(
	UShopComponent& Shop,
	const FShopItemDefinition& Definition)
{
	UWeaponInventoryComponent* Inventory = GetWeaponInventory();
	const FGameplayTag WeaponTag = Definition.GrantedWeaponTag;

	if (!IsValid(Inventory) || !Inventory->IsPossessed(WeaponTag))
	{
		return false;
	}

	// 이미 해금된 업그레이드는 결제 전에 차단한다.
	if (Inventory->IsUnlocked(WeaponTag))
	{
		return false;
	}

	const FShopTransactionResult Result = Shop.TryBeginTransaction(Definition.ItemId);
	if (!Result.WasSuccessful())
	{
		return false;
	}

	// EffectClass가 없는 업그레이드는 해금만 확정한다.
	FActiveGameplayEffectHandle EffectHandle;
	if (!Definition.EffectClass.IsNull())
	{
		EffectHandle = ApplyEffect(Definition);
		if (!EffectHandle.WasSuccessfullyApplied())
		{
			Shop.RefundTransaction(Result.Transaction);
			return false;
		}
	}

	if (Shop.CommitTransaction(Result.Transaction) != EShopPurchaseResult::Success)
	{
		// Commit 실패 시 적용한 효과부터 되돌린 뒤 거래를 환불한다.
		if (EffectHandle.WasSuccessfullyApplied())
		{
			URogueAbilitySystemComponent* ASC = GetAbilitySystem();
			if (!IsValid(ASC) || !ASC->RemoveActiveGameplayEffect(EffectHandle))
			{
				UE_LOG(LogShopPurchase, Error,
					TEXT("Weapon upgrade effect rollback failed: Player=%s Item=%s"),
					*GetNameSafe(GetOwner()),
					*Definition.ItemId.ToString());
				return false;
			}
		}

		Shop.RefundTransaction(Result.Transaction);
		return false;
	}

	if (!Inventory->ServerConfirmWeaponUpgrade(WeaponTag))
	{
		UE_LOG(LogShopPurchase, Error,
			TEXT("Weapon upgrade confirmation failed after transaction commit: Player=%s Weapon=%s"),
			*GetNameSafe(GetOwner()),
			*WeaponTag.ToString());
		return false;
	}

	UE_LOG(LogShopPurchase, Log, TEXT("Weapon upgraded: Player=%s Weapon=%s"),
		*GetNameSafe(GetOwner()),
		*WeaponTag.ToString());

	return true;
}

// --------------------------------------------------
// HP / Damage

bool UShopPurchaseComponent::TryPurchaseStatUpgrade(
	UShopComponent& Shop,
	const FShopItemDefinition& Definition)
{
	const FShopTransactionResult Result = Shop.TryBeginTransaction(Definition.ItemId);
	if (!Result.WasSuccessful())
	{
		return false;
	}

	const FActiveGameplayEffectHandle EffectHandle = ApplyEffect(Definition);
	if (!EffectHandle.WasSuccessfullyApplied())
	{
		Shop.RefundTransaction(Result.Transaction);
		return false;
	}

	if (Shop.CommitTransaction(Result.Transaction) != EShopPurchaseResult::Success)
	{
		// Commit 실패 시 적용한 효과부터 되돌린 뒤 거래를 환불한다.
		URogueAbilitySystemComponent* ASC = GetAbilitySystem();
		if (!IsValid(ASC) || !ASC->RemoveActiveGameplayEffect(EffectHandle))
		{
			UE_LOG(LogShopPurchase, Error,
				TEXT("Stat upgrade effect rollback failed: Player=%s Item=%s"),
				*GetNameSafe(GetOwner()),
				*Definition.ItemId.ToString());
			return false;
		}

		Shop.RefundTransaction(Result.Transaction);
		return false;
	}

	return true;
}

// --------------------------------------------------
// 보호막
//
// 거래는 Pending으로 유지하되 GE는 즉시 적용한다. 취소/교체 시 PendingShieldEffect로 되돌린다.

bool UShopPurchaseComponent::TryPurchaseShield(
	UShopComponent& Shop,
	const FShopItemDefinition& Definition)
{
	if (PendingShieldTransaction.IsValid()
		&& PendingShieldTransaction.ItemId.MatchesTagExact(Definition.ItemId))
	{
		return true;
	}

	const FShopTransactionResult Result = PendingShieldTransaction.IsValid()
		? Shop.TryReplaceTransaction(PendingShieldTransaction, Definition.ItemId)
		: Shop.TryBeginTransaction(Definition.ItemId);

	if (!Result.WasSuccessful())
	{
		return false;
	}

	// 결제가 성공한 시점부터 서버 원장과 같은 거래 핸들을 보관한다.
	// 이후 GE 적용이나 환불이 실패해도 다음 취소 요청에서 해당 거래를 다시 찾을 수 있다.
	PendingShieldTransaction = Result.Transaction;

	// 교체 시 이전 Pending GE를 제거한 뒤 새 GE를 적용한다.
	RemovePendingShieldEffect();

	const FActiveGameplayEffectHandle EffectHandle = ApplyEffect(Definition);
	if (!EffectHandle.WasSuccessfullyApplied())
	{
		RefundPendingShield(Shop);
		RefreshPendingMirror();
		return false;
	}

	PendingShieldEffect = EffectHandle;
	RefreshPendingMirror();
	return true;
}

bool UShopPurchaseComponent::RefundPendingWeapon(UShopComponent& Shop, const FGameplayTag ItemId)
{
	const FShopTransactionHandle* Transaction = PendingWeaponTransactions.Find(ItemId);
	if (!Transaction)
	{
		return false;
	}

	// 환불 실패 시 재시도할 수 있도록 Pending을 유지한다.
	if (Shop.RefundTransaction(*Transaction) != EShopPurchaseResult::Success)
	{
		return false;
	}

	PendingWeaponTransactions.Remove(ItemId);
	return true;
}

bool UShopPurchaseComponent::RestorePrimaryAfterPendingRefund(UShopComponent& Shop)
{
	PendingWeaponItemId = FGameplayTag();

	const UWeaponInventoryComponent* Inventory = GetWeaponInventory();
	if (!IsValid(Inventory)
		|| Inventory->GetEquippedPrimaryTag().IsValid()
		|| PendingWeaponTransactions.IsEmpty())
	{
		return RogueWeaponLoadout::RestoreConfirmedPrimary(GetPlayerState(), GetOwningPawn());
	}

	TArray<FGameplayTag> RemainingItemIds;
	PendingWeaponTransactions.GetKeys(RemainingItemIds);
	RemainingItemIds.Sort(
		[](const FGameplayTag& Left, const FGameplayTag& Right)
		{
			return Left.ToString() < Right.ToString();
		});

	FShopItemDefinition FallbackDefinition;
	if (!Shop.FindItemDefinition(RemainingItemIds[0], FallbackDefinition)
		|| !RogueWeaponLoadout::ApplyPrimaryWeapon(
			GetOwningPawn(),
			FallbackDefinition.GrantedWeaponTag))
	{
		return RogueWeaponLoadout::RestoreConfirmedPrimary(GetPlayerState(), GetOwningPawn());
	}

	PendingWeaponItemId = RemainingItemIds[0];
	return true;
}

bool UShopPurchaseComponent::RefundPendingShield(UShopComponent& Shop)
{
	if (!PendingShieldTransaction.IsValid())
	{
		return false;
	}

	if (Shop.RefundTransaction(PendingShieldTransaction) != EShopPurchaseResult::Success)
	{
		return false;
	}

	PendingShieldTransaction = FShopTransactionHandle();

	// 환불된 보호막의 Pending GE도 제거한다.
	RemovePendingShieldEffect();
	return true;
}

void UShopPurchaseComponent::ServerRefundPendingItem_Implementation(const FGameplayTag ItemId)
{
	AShooterPlayerState* PlayerState = GetPlayerState();
	UShopComponent* Shop = IsValid(PlayerState) ? PlayerState->GetShopComponent() : nullptr;

	// Pending 환불은 준비 페이즈에서만 허용한다.
	if (!IsValid(Shop) || !Shop->IsPurchaseWindowOpen())
	{
		return;
	}

	FShopItemDefinition Definition;
	if (!Shop->FindItemDefinition(ItemId, Definition))
	{
		return;
	}

	switch (Definition.ItemType)
	{
	case EShopItemType::Weapon:
	{
		// 장착 중인 Pending 무기라면 환불 후 마지막 확정 장착으로 복원한다.
		const bool bWasEquipped = PendingWeaponItemId.MatchesTagExact(ItemId);

		if (RefundPendingWeapon(*Shop, ItemId))
		{
			if (bWasEquipped && !RestorePrimaryAfterPendingRefund(*Shop))
			{
				UE_LOG(LogShopPurchase, Error,
					TEXT("Failed to restore primary after pending refund: Player=%s Item=%s"),
					*GetNameSafe(PlayerState),
					*ItemId.ToString());
			}

			RefreshPendingMirror();
		}
		break;
	}

	case EShopItemType::Shield:
		// 요청한 상품과 현재 Pending Shield가 일치할 때만 환불한다.
		if (PendingShieldTransaction.IsValid()
			&& PendingShieldTransaction.ItemId.MatchesTagExact(ItemId)
			&& RefundPendingShield(*Shop))
		{
			RefreshPendingMirror();
		}
		break;

	default:
		// 즉시 확정 상품은 Pending 환불 대상이 아니다.
		break;
	}
}

void UShopPurchaseComponent::RemovePendingShieldEffect()
{
	if (!PendingShieldEffect.IsValid())
	{
		return;
	}

	if (URogueAbilitySystemComponent* ASC = GetAbilitySystem())
	{
		ASC->RemoveActiveGameplayEffect(PendingShieldEffect);
	}

	PendingShieldEffect = FActiveGameplayEffectHandle();
}

URogueAbilitySystemComponent* UShopPurchaseComponent::GetAbilitySystem() const
{
	const AShooterPlayerState* PlayerState = GetPlayerState();
	return IsValid(PlayerState) ? PlayerState->GetRogueAbilitySystemComponent() : nullptr;
}

FActiveGameplayEffectHandle UShopPurchaseComponent::ApplyEffect(const FShopItemDefinition& Definition)
{
	URogueAbilitySystemComponent* ASC = GetAbilitySystem();

	if (!IsValid(ASC) || Definition.EffectClass.IsNull())
	{
		return FActiveGameplayEffectHandle();
	}

	UClass* EffectClass = Definition.EffectClass.LoadSynchronous();
	const UGameplayEffect* Effect = EffectClass ? EffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;

	if (!Effect)
	{
		return FActiveGameplayEffectHandle();
	}

	// Instant GE는 Active 핸들이 없어 Pending 취소나 라운드 정리에서 제거할 수 없으므로 거부한다.
	if (Effect->DurationPolicy == EGameplayEffectDurationType::Instant)
	{
		UE_LOG(LogShopPurchase, Error,
			TEXT("Instant shop effect cannot be tracked for rollback: Effect=%s Item=%s"),
			*GetNameSafe(EffectClass),
			*Definition.ItemId.ToString());

		return FActiveGameplayEffectHandle();
	}

	// TierPrices는 가격 단계이며 GE 레벨과는 별개다. 스탯 상품은 같은 레벨의 GE를
	// 구매 횟수만큼 누적하고, 실제 누적 규칙은 해당 GE가 소유한다.
	return ASC->ApplyGameplayEffectToSelf(Effect, 1.f, ASC->MakeEffectContext());
}

// --------------------------------------------------
// 라운드 시작 확정

bool UShopPurchaseComponent::CanCommitPendingPurchases() const
{
	const AShooterPlayerState* PlayerState = GetPlayerState();
	const UShopComponent* Shop = IsValid(PlayerState) ? PlayerState->GetShopComponent() : nullptr;
	if (!IsValid(Shop))
	{
		return false;
	}

	if (!PendingWeaponTransactions.IsEmpty())
	{
		TArray<FPendingWeaponCommit> PendingCommits;
		FGameplayTag DesiredEquippedPrimary;
		if (!BuildPendingWeaponCommitPlan(*Shop, PendingCommits, DesiredEquippedPrimary))
		{
			return false;
		}
	}

	return CanCommitPendingShield(*Shop);
}

bool UShopPurchaseComponent::CommitPendingPurchases()
{
	AShooterPlayerState* PlayerState = GetPlayerState();
	UShopComponent* Shop = IsValid(PlayerState) ? PlayerState->GetShopComponent() : nullptr;

	if (!IsValid(Shop))
	{
		return false;
	}

	TArray<FPendingWeaponCommit> PendingCommits;
	FGameplayTag DesiredEquippedPrimary;
	if (!PendingWeaponTransactions.IsEmpty()
		&& !BuildPendingWeaponCommitPlan(*Shop, PendingCommits, DesiredEquippedPrimary))
	{
		RefreshPendingMirror();
		return false;
	}

	if (!CanCommitPendingShield(*Shop))
	{
		RefreshPendingMirror();
		return false;
	}

	bool bCommitted = true;
	if (!PendingCommits.IsEmpty())
	{
		bCommitted = CommitPendingWeapons(*Shop, PendingCommits, DesiredEquippedPrimary);
	}

	if (bCommitted && PendingShieldTransaction.IsValid())
	{
		bCommitted = CommitPendingShield(*Shop);
	}

	RefreshPendingMirror();
	return bCommitted;
}

bool UShopPurchaseComponent::BuildPendingWeaponCommitPlan(
	const UShopComponent& Shop,
	TArray<FPendingWeaponCommit>& OutPendingCommits,
	FGameplayTag& OutDesiredEquippedPrimary) const
{
	const UWeaponInventoryComponent* Inventory = GetWeaponInventory();
	if (!IsValid(Inventory))
	{
		return false;
	}

	OutPendingCommits.Reset(PendingWeaponTransactions.Num());

	// Inventory의 기존 확정 API가 소유권과 장착을 함께 갱신하므로,
	// 확정 소유 무기는 Inventory 상태를, Pending 무기는 ItemId를 기준으로 최종 장착을 보존한다.
	OutDesiredEquippedPrimary = Inventory->GetEquippedPrimaryTag();

	for (const TPair<FGameplayTag, FShopTransactionHandle>& Pair : PendingWeaponTransactions)
	{
		FShopItemDefinition Definition;
		if (!Pair.Key.IsValid()
			|| !Pair.Value.IsValid()
			|| !Pair.Value.ItemId.MatchesTagExact(Pair.Key)
			|| Shop.CanCommitTransaction(Pair.Value) != EShopPurchaseResult::Success
			|| !Shop.FindItemDefinition(Pair.Key, Definition)
			|| !UWeaponInventoryComponent::IsPrimaryWeaponTag(Definition.GrantedWeaponTag))
		{
			UE_LOG(LogShopPurchase, Error,
				TEXT("Weapon commit prevalidation failed: Player=%s Item=%s"),
				*GetNameSafe(GetOwner()),
				*Pair.Key.ToString());
			return false;
		}

		FPendingWeaponCommit& Pending = OutPendingCommits.AddDefaulted_GetRef();
		Pending.ItemId = Pair.Key;
		Pending.Transaction = Pair.Value;
		Pending.WeaponTag = Definition.GrantedWeaponTag;

		if (PendingWeaponItemId.MatchesTagExact(Pair.Key))
		{
			OutDesiredEquippedPrimary = Definition.GrantedWeaponTag;
		}
	}

	// TMap 순회 순서와 무관하게 서버 확정과 로그 순서를 고정한다.
	OutPendingCommits.Sort(
		[](const FPendingWeaponCommit& Left, const FPendingWeaponCommit& Right)
		{
			return Left.ItemId.ToString() < Right.ItemId.ToString();
		});

	if (!OutPendingCommits.IsEmpty() && !OutDesiredEquippedPrimary.IsValid())
	{
		UE_LOG(LogShopPurchase, Error,
			TEXT("Weapon commit has pending purchases without a selected primary: Player=%s"),
			*GetNameSafe(GetOwner()));
		return false;
	}

	if (OutDesiredEquippedPrimary.IsValid()
		&& !Inventory->IsPossessed(OutDesiredEquippedPrimary)
		&& !OutPendingCommits.ContainsByPredicate(
			[OutDesiredEquippedPrimary](const FPendingWeaponCommit& Pending)
			{
				return Pending.WeaponTag == OutDesiredEquippedPrimary;
			}))
	{
		UE_LOG(LogShopPurchase, Error,
			TEXT("Weapon commit has an invalid desired primary: Player=%s Weapon=%s"),
			*GetNameSafe(GetOwner()),
			*OutDesiredEquippedPrimary.ToString());
		return false;
	}

	return true;
}

bool UShopPurchaseComponent::CanCommitPendingShield(const UShopComponent& Shop) const
{
	return !PendingShieldTransaction.IsValid()
		|| (PendingShieldEffect.IsValid()
			&& Shop.CanCommitTransaction(PendingShieldTransaction) == EShopPurchaseResult::Success);
}

bool UShopPurchaseComponent::CommitPendingWeapons(
	UShopComponent& Shop,
	const TArray<FPendingWeaponCommit>& PendingCommits,
	const FGameplayTag DesiredEquippedPrimary)
{
	UWeaponInventoryComponent* Inventory = GetWeaponInventory();
	if (!IsValid(Inventory))
	{
		return false;
	}

	for (const FPendingWeaponCommit& Pending : PendingCommits)
	{
		if (Shop.CommitTransaction(Pending.Transaction) != EShopPurchaseResult::Success)
		{
			UE_LOG(LogShopPurchase, Error,
				TEXT("Weapon transaction commit failed: Player=%s Item=%s"),
				*GetNameSafe(GetOwner()),
				*Pending.ItemId.ToString());
			return false;
		}

		// 사전 검증된 Primary 소유권을 Inventory에 확정한다.
		if (!Inventory->ServerConfirmPrimaryPurchase(Pending.WeaponTag))
		{
			UE_LOG(LogShopPurchase, Error,
				TEXT("Weapon ownership confirmation failed after transaction commit: Player=%s Weapon=%s"),
				*GetNameSafe(GetOwner()),
				*Pending.WeaponTag.ToString());
			return false;
		}

		PendingWeaponTransactions.Remove(Pending.ItemId);

		UE_LOG(LogShopPurchase, Log,
			TEXT("Weapon ownership confirmed: Player=%s Weapon=%s"),
			*GetNameSafe(GetOwner()),
			*Pending.WeaponTag.ToString());
	}

	if (!Inventory->ServerSetEquippedPrimary(DesiredEquippedPrimary))
	{
		UE_LOG(LogShopPurchase, Error,
			TEXT("Failed to restore selected primary after ownership commit: Player=%s Weapon=%s"),
			*GetNameSafe(GetOwner()),
			*DesiredEquippedPrimary.ToString());
		return false;
	}

	PendingWeaponItemId = FGameplayTag();
	return true;
}

// 보호막 GE는 구매 시 이미 적용되어 있으므로 거래만 확정한다.
bool UShopPurchaseComponent::CommitPendingShield(UShopComponent& Shop)
{
	if (Shop.CommitTransaction(PendingShieldTransaction) != EShopPurchaseResult::Success)
	{
		UE_LOG(LogShopPurchase, Error,
			TEXT("Shield commit failed: Player=%s Item=%s"),
			*GetNameSafe(GetOwner()),
			*PendingShieldTransaction.ItemId.ToString());

		return false;
	}

	// 확정 후에는 rollback 대상이 아니므로 Pending 거래와 GE 핸들을 비운다.
	PendingShieldTransaction = FShopTransactionHandle();
	PendingShieldEffect = FActiveGameplayEffectHandle();
	return true;
}

// --------------------------------------------------
// Pending 구매 취소

bool UShopPurchaseComponent::CancelPendingPurchases()
{
	AShooterPlayerState* PlayerState = GetPlayerState();
	UShopComponent* Shop = IsValid(PlayerState) ? PlayerState->GetShopComponent() : nullptr;

	if (!IsValid(Shop))
	{
		return false;
	}

	bool bCancelled = true;

	// 주무기 Pending을 개별 환불한다. 실패한 거래는 재시도할 수 있도록 그대로 남긴다.
	if (!PendingWeaponTransactions.IsEmpty())
	{
		TArray<FGameplayTag> PendingItemIds;
		PendingWeaponTransactions.GetKeys(PendingItemIds);

		for (const FGameplayTag& PendingItemId : PendingItemIds)
		{
			if (!RefundPendingWeapon(*Shop, PendingItemId))
			{
				bCancelled = false;
			}
		}

		// 소유 상태는 아직 확정 전이므로 런타임 장착만 마지막 확정 상태로 복원한다.
		PendingWeaponItemId = FGameplayTag();
		RogueWeaponLoadout::RestoreConfirmedPrimary(PlayerState, GetOwningPawn());
	}

	if (PendingShieldTransaction.IsValid() && !RefundPendingShield(*Shop))
	{
		bCancelled = false;
	}

	RefreshPendingMirror();
	return bCancelled;
}
