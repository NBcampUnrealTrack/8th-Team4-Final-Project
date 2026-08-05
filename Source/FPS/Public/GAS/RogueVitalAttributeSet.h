// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"

#include "RogueVitalAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRogueOutOfHealth,
	UAbilitySystemComponent* /*VictimASC*/, const FGameplayEffectContextHandle& /*Context*/);

UCLASS()
class FPS_API URogueVitalAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	URogueVitalAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	bool IsDeathDeclared() const { return bDeathDeclared; }

	// 라운드 시작 / 부활 시 반드시 호출
	void ClearDeathDeclared() { bDeathDeclared = false; }

protected:
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Shield(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxShield(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ShopDamageAmplification(const FGameplayAttributeData& OldValue);

private:
	// Max 계열이 바뀔 때 현재값도 수정
	void AdjustCurrentForNewMax(const FGameplayAttribute& CurrentAttribute,
	                            float CurrentValue, float OldMax, float NewMax);

public:
	// 0 이 되면 즉시 사망
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Rogue|Vital")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(URogueVitalAttributeSet, Health)

	// 기본 100. 상점 5회 강화로 200, 증강으로 추가 상향
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Rogue|Vital")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(URogueVitalAttributeSet, MaxHealth)

	// 보호막 / 피격 시 체력보다 우선 차감
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Shield, Category = "Rogue|Vital")
	FGameplayAttributeData Shield;
	ATTRIBUTE_ACCESSORS(URogueVitalAttributeSet, Shield)

	// 최대 보호막 / 해당 값은 조정되지 않음
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxShield, Category = "Rogue|Vital")
	FGameplayAttributeData MaxShield;
	ATTRIBUTE_ACCESSORS(URogueVitalAttributeSet, MaxShield)

	// 상점 공격력 강화
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ShopDamageAmplification, Category = "Rogue|Vital")
	FGameplayAttributeData ShopDamageAmplification;
	ATTRIBUTE_ACCESSORS(URogueVitalAttributeSet, ShopDamageAmplification)

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Vital")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(URogueVitalAttributeSet, IncomingDamage)

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Vital")
	FGameplayAttributeData IncomingShieldDamage;
	ATTRIBUTE_ACCESSORS(URogueVitalAttributeSet, IncomingShieldDamage)

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Vital")
	FGameplayAttributeData IncomingHealing;
	ATTRIBUTE_ACCESSORS(URogueVitalAttributeSet, IncomingHealing)

private:
	// 중복 호출 방지를 위한 사망 여부 플래그
	bool bDeathDeclared = false;

public:
	// 사망 확정 시 1회 발신. 구독자가 사망 시퀀스를 소유한다
	FOnRogueOutOfHealth OnOutOfHealth;
};
