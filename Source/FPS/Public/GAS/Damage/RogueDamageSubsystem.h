// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "GAS/Damage/RogueDamageTypes.h"

#include "RogueDamageSubsystem.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class URogueVitalAttributeSet;

struct FRogueDamageTarget
{
	UAbilitySystemComponent* ASC = nullptr;
	const URogueVitalAttributeSet* Vital = nullptr;
};

UCLASS()
class FPS_API URogueDamageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 신규 히트스캔 경로
	UFUNCTION(BlueprintCallable, Category = "Rogue|Damage")
	FRogueDamageResult ApplyHitscanDamage(const FRogueDamageParams& Params, const FRogueHitContext& Hit);

	// 탄착 기하가 없는 피해, 스킬, 폭발, 자가피해
	UFUNCTION(BlueprintCallable, Category = "Rogue|Damage")
	FRogueDamageResult ApplyFlatDamage(const FRogueDamageParams& Params, AActor* Target, float Damage);

	// 자기 체력을 대가로 지불, 남는 체력이 최소 1이 되도록 클램핑, 감소한 체력만큼 반환 
	UFUNCTION(BlueprintCallable, Category = "Rogue|Damage")
	float ApplyHealthCost(AActor* Payer, float Amount, float MinHealthRemaining = 1.f);

private:
	// 두 진입점의 공통 흐름
	FRogueDamageResult ApplyDamageInternal(const FRogueDamageParams& Params,
	                                       AActor* TargetActor,
	                                       const FRogueHitContext* Hit,
	                                       float FlatDamage);

	// 액터로부터 ASC를 얻음
	static UAbilitySystemComponent* ResolveASC(AActor* Actor);

	// 데미지 적용 가능 여부 확인
	static bool ResolveDamageTarget(AActor* TargetActor, FRogueDamageTarget& OutTarget);

	// GE 클래스 결정
	static UClass* ResolveDamageEffectClass();

	// EffectContextHandle 생성. Hit이 nullptr이면 FlatDamage
	FGameplayEffectContextHandle MakeDamageContext(const FRogueDamageParams& Params,
	                                               const FRogueHitContext* Hit,
	                                               UAbilitySystemComponent& ContextASC,
	                                               const URogueVitalAttributeSet* TargetVital) const;

	// 데미지 적용 결과 처리
	static FRogueDamageResult MeasureDamageResult(const URogueVitalAttributeSet& Vital,
	                                              float ShieldBefore,
	                                              float HealthBefore);

	// 조건부 증강 규칙이 필요로 하는 태그 조립
	static void BuildContextTags(const FRogueDamageParams& Params,
	                             const FRogueHitContext* Hit,
	                             const URogueVitalAttributeSet* TargetVital,
	                             FGameplayTagContainer& OutTags);

	// 준 피해에 비례해 가해자를 회복
	void ApplyLifesteal(UAbilitySystemComponent* SourceASC,
	                    const FRogueDamageTarget& DamageTarget,
	                    const FRogueDamageResult& Result) const;

	// SetByCaller Data.Magnitude.Primary 만 넣는 Instant GE 를 자신에게 적용
	static void ApplyInstantMagnitude(UAbilitySystemComponent& ASC,
	                                  const TSoftClassPtr<UGameplayEffect>& EffectClass,
	                                  float Magnitude);

	// 최종 결과 이벤트 발신
	void BroadcastCombatEvents(const FRogueDamageParams& Params,
	                           AActor* TargetActor,
	                           const FRogueDamageResult& Result,
	                           const FGameplayEffectContextHandle& ContextHandle) const;
};
