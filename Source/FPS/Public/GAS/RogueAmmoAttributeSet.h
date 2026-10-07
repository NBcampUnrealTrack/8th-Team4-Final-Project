#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "RogueAmmoAttributeSet.generated.h"

class AWeaponBase;

// GAS 지급 요청만 처리한다. 실제 보유 탄약은 각 무기가 관리한다.
UCLASS()
class FPS_API URogueAmmoAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	URogueAmmoAttributeSet();

	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// GE 적용 조건과 지급 처리에서 같은 대상/수치 검증을 사용한다.
	static AWeaponBase* ResolveReserveAmmoTarget(
		const UAbilitySystemComponent& AbilitySystem, const FGameplayEffectContextHandle& EffectContext,
		float Magnitude, int32& OutAmount);

	// 일회성 메타 속성. 처리 직전에 비우며 복제하지 않는다.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Rogue|Ammo")
	FGameplayAttributeData IncomingReserveAmmo;
	ATTRIBUTE_ACCESSORS_BASIC(URogueAmmoAttributeSet, IncomingReserveAmmo)
};
