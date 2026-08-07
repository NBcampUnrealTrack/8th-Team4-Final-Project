// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Augment/AugmentAssembler.h"
#include "Augment/AugmentTypes.h"
#include "AugmentInventoryComponent.generated.h"

class UAbilitySystemComponent;
class UAugmentDefinition;
class UAugmentRegistrySubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAugmentInventoryChanged);

UCLASS(ClassGroup = (Rogue), meta = (BlueprintSpawnableComponent))
class FPS_API UAugmentInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAugmentInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ASC 는 PlayerState 에 있어 이 컴포넌트의 BeginPlay 시점에 없을 수 있음
	// 소유자가 InitAbilityActorInfo 직후에 직접 호출
	UFUNCTION(BlueprintCallable, Category = "Rogue|Augment")
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Augment")
	void UninitializeFromAbilitySystem();

	// 증강 획득. 서버 전용
	// 같은 계열을 이미 보유하고 있으면 하위 레벨을 걷어내고 교체
	UFUNCTION(BlueprintCallable, Category = "Rogue|Augment")
	bool ServerAcquireAugment(UAugmentDefinition* Definition);

	// 매치 초기화. 서버 전용
	UFUNCTION(BlueprintCallable, Category = "Rogue|Augment")
	void ServerRemoveAllAugments();

	// 현재 보유한 증강 목록, 서버에서만 변경하며 클라이언트에게 복제
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	TArray<FAugmentInventoryEntry> GetEntries() const { return Entries; }

	// 정의까지 해석한 목록
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	TArray<UAugmentDefinition*> GetOwnedDefinitions() const;

	// 이 계열의 보유 레벨. 미보유면 0.
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	int32 GetLevelInLine(FGameplayTag LineTag) const;

	// ExecCalc 이 히트마다 읽는다. 정의를 매번 훑지 않도록 캐싱
	const TArray<FAugmentConditionalDamageRule>& GetConditionalDamageRules() const { return CachedConditionalRules; }

	// ExecCalc 은 ASC 만 들고 있다. 거기서 인벤토리로 건너오는 유일한 경로
	static UAugmentInventoryComponent* FindInventory(const UAbilitySystemComponent* ASC);

protected:
	virtual void BeginPlay() override;
	virtual void OnUnregister() override;

private:
	UFUNCTION()
	void OnRep_Entries();

	UFUNCTION()
	void HandleRegistryReady();

	UAugmentRegistrySubsystem* GetRegistry() const;
	UAugmentDefinition* ResolveDefinition(const FPrimaryAssetId& DefinitionId) const;

	int32 FindEntryIndexInLine(const FGameplayTag& LineTag) const;
	void RebuildConditionalRuleCache();

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// 보유 목록. 서버에서만 변경하고 소유자에게만 복제
	UPROPERTY(ReplicatedUsing = OnRep_Entries)
	TArray<FAugmentInventoryEntry> Entries;

	// 서버 전용. 계열별 해제 핸들
	UPROPERTY()
	TMap<FGameplayTag, FAugmentGrantHandles> GrantHandlesByLine;

	TArray<FAugmentConditionalDamageRule> CachedConditionalRules;

public:
	// 증강 획득/해제 시점에 브로드캐스트. 서버에서만 발생
	UPROPERTY(BlueprintAssignable, Category = "Rogue|Augment")
	FOnAugmentInventoryChanged OnInventoryChanged;
};
