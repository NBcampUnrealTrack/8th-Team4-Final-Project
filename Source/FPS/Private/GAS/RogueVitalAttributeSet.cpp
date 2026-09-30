// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/RogueVitalAttributeSet.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GameplayTags.h"

URogueVitalAttributeSet::URogueVitalAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitShield(0.f);
	InitMaxShield(0.f);
	InitShopDamageAmplification(0.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
}

void URogueVitalAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 체력/보호막은 관전자, 팀원 UI 에도 필요
	DOREPLIFETIME_CONDITION_NOTIFY(URogueVitalAttributeSet, Health,    COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueVitalAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueVitalAttributeSet, Shield,    COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueVitalAttributeSet, MaxShield, COND_None, REPNOTIFY_Always);

	// 본인 HUD 전용
	DOREPLIFETIME_CONDITION_NOTIFY(URogueVitalAttributeSet, ShopDamageAmplification, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueVitalAttributeSet, Stamina,    COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URogueVitalAttributeSet, MaxStamina, COND_OwnerOnly, REPNOTIFY_Always);
}

void URogueVitalAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 최대치는 정수로 유지
	if (Attribute == GetMaxHealthAttribute())
	{
		// 최대 체력은 반드시 1 이상
		NewValue = FMath::Max(1.f, FMath::RoundToFloat(NewValue));
	}
	else if (Attribute == GetMaxShieldAttribute())
	{
		NewValue = FMath::Max(0.f, FMath::RoundToFloat(NewValue));
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetShieldAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxShield());
	}
	else if (Attribute == GetShopDamageAmplificationAttribute())
	{
		// 상점 공격력 증가 상한 +25%
		NewValue = FMath::Clamp(NewValue, 0.f, 0.25f);
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(1.f, FMath::RoundToFloat(NewValue));
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	}
}

void URogueVitalAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		AdjustCurrentForNewMax(GetHealthAttribute(), GetHealth(), OldValue, NewValue);
	}
	else if (Attribute == GetMaxShieldAttribute())
	{
		AdjustCurrentForNewMax(GetShieldAttribute(), GetShield(), OldValue, NewValue);
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		AdjustCurrentForNewMax(GetStaminaAttribute(), GetStamina(), OldValue, NewValue);
	}
}

void URogueVitalAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	const FGameplayAttribute Attribute = Data.EvaluatedData.Attribute;

	// 보호막 차감
	if (Attribute == GetIncomingShieldDamageAttribute())
	{
		const float Delta = GetIncomingShieldDamage();
		SetIncomingShieldDamage(0.f);

		if (Delta > 0.f)
		{
			SetShield(FMath::Clamp(GetShield() - Delta, 0.f, GetMaxShield()));
		}
		return;
	}

	// 체력 차감
	if (Attribute == GetIncomingDamageAttribute())
	{
		const float Delta = GetIncomingDamage();
		SetIncomingDamage(0.f);

		if (Delta > 0.f)
		{
			float NewHealth = FMath::Clamp(GetHealth() - Delta, 0.f, GetMaxHealth());

			// 치명 피해를 목숨당 1번 체력 1로 버팀
			if (NewHealth <= 0.f && CanSurviveLethal())
			{
				NewHealth = 1.f;
				bLethalSurviveUsed = true;

				if (AActor* Avatar = GetOwningAbilitySystemComponent()->GetAvatarActor())
				{
					FGameplayEventData Payload;
					Payload.EventTag   = RogueTags::Event_Combat_LethalSurvived;
					Payload.Instigator = Data.EffectSpec.GetContext().GetInstigator();
					Payload.Target     = Avatar;
					UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Avatar, RogueTags::Event_Combat_LethalSurvived, Payload);
				}
			}

			SetHealth(NewHealth);
		}

		TryDeclareDeath(Data.EffectSpec.GetContext());
		return;
	}

	// 회복
	if (Attribute == GetIncomingHealingAttribute())
	{
		const float Delta = GetIncomingHealing();
		SetIncomingHealing(0.f);

		// 이미 사망한 상태면 회복하지 않음
		if (Delta > 0.f && !bDeathDeclared)
		{
			SetHealth(FMath::Clamp(GetHealth() + Delta, 0.f, GetMaxHealth()));
		}
		return;
	}

	// 보호막 회복
	if (Attribute == GetIncomingShieldHealingAttribute())
	{
		const float Delta = GetIncomingShieldHealing();
		SetIncomingShieldHealing(0.f);

		// 체력 회복과 같은 기준. 사망한 상태면 보호막도 채우지 않음
		if (Delta > 0.f && !bDeathDeclared)
		{
			SetShield(FMath::Clamp(GetShield() + Delta, 0.f, GetMaxShield()));
		}
		return;
	}

	// GE가 직접 Health/Shield 를 수정하는 경우 클램핑
	// 해당 경로는 사용하지 않는 것을 권장, 일단 구현
	if (Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));

		TryDeclareDeath(Data.EffectSpec.GetContext());
	}
	else if (Attribute == GetShieldAttribute())
	{
		SetShield(FMath::Clamp(GetShield(), 0.f, GetMaxShield()));
	}
	else if (Attribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.f, GetMaxStamina()));
	}
}

void URogueVitalAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URogueVitalAttributeSet, Health, OldValue);
}

void URogueVitalAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URogueVitalAttributeSet, MaxHealth, OldValue);
}

void URogueVitalAttributeSet::OnRep_Shield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URogueVitalAttributeSet, Shield, OldValue);
}

void URogueVitalAttributeSet::OnRep_MaxShield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URogueVitalAttributeSet, MaxShield, OldValue);
}

void URogueVitalAttributeSet::OnRep_ShopDamageAmplification(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URogueVitalAttributeSet, ShopDamageAmplification, OldValue);
}

void URogueVitalAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URogueVitalAttributeSet, Stamina, OldValue);
}

void URogueVitalAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URogueVitalAttributeSet, MaxStamina, OldValue);
}

void URogueVitalAttributeSet::ServerForceDeath(const FGameplayEffectContextHandle& Context)
{
	UAbilitySystemComponent* OwnerASC = GetOwningAbilitySystemComponent();
	if (!ensure(OwnerASC && OwnerASC->IsOwnerActorAuthoritative()) || bDeathDeclared)
	{
		return;
	}

	SetHealth(0.f);
	TryDeclareDeath(Context);
}

void URogueVitalAttributeSet::TryDeclareDeath(const FGameplayEffectContextHandle& Context)
{
	if (GetHealth() > 0.f || bDeathDeclared)
	{
		return;
	}

	// 사망 확정은 서버에서만
	UAbilitySystemComponent* OwnerASC = GetOwningAbilitySystemComponent();
	if (!OwnerASC || !OwnerASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	bDeathDeclared = true;

	// 사망시 HealthComponent 의 OnOutOfHealth 이벤트를 발신
	// 구독되지 않은 상태면 사망 처리가 정상적으로 진행되지 않음
	ensureMsgf(OnOutOfHealth.IsBound(),
		TEXT("[RogueVital] %s: 사망했지만 OnOutOfHealth 구독자가 없습니다. "
		     "URogueHealthComponent 부착과 InitializeWithAbilitySystem 호출을 확인하세요."),
		*GetNameSafe(OwnerASC->GetAvatarActor()));

	OnOutOfHealth.Broadcast(OwnerASC, Context);
}

bool URogueVitalAttributeSet::CanSurviveLethal() const
{
	const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();

	return !bLethalSurviveUsed && !bDeathDeclared && ASC && ASC->IsOwnerActorAuthoritative()
		&& ASC->HasMatchingGameplayTag(RogueTags::State_Augment_LethalSurvive);
}

void URogueVitalAttributeSet::AdjustCurrentForNewMax(const FGameplayAttribute& CurrentAttribute,
                                                     float CurrentValue, float OldMax, float NewMax)
{
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC || FMath::IsNearlyEqual(OldMax, NewMax))
	{
		return;
	}

	// 사망한 상태면 체력/보호막 수정 X
	if (bDeathDeclared)
	{
		return;
	}

	if (NewMax > OldMax)
	{
		// 증가분만큼 현재값도 증가
		ASC->ApplyModToAttributeUnsafe(CurrentAttribute, EGameplayModOp::Additive, NewMax - OldMax);
	}
	else if (CurrentValue > NewMax)
	{
		// 감소 시에는 넘치는 값만 감소
		ASC->ApplyModToAttributeUnsafe(CurrentAttribute, EGameplayModOp::Additive, NewMax - CurrentValue);
	}
}
