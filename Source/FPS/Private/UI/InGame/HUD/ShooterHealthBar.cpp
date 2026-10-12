// ShooterHealthBar.cpp
#include "UI/InGame/HUD/ShooterHealthBar.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "UI/Common/ShooterGauge.h"
#include "UI/InGame/HUD/ShooterVitalBar.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterHealthBar, Log, All);

void UShooterHealthBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UShooterHealthBar::NativeDestruct()
{
	Super::NativeDestruct();
}

void UShooterHealthBar::OnAbilitySystemReady(URogueAbilitySystemComponent* ASC)
{
	ASC->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HandleMaxHealthChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetShieldAttribute()).AddUObject(this, &ThisClass::HandleShieldChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetMaxShieldAttribute()).AddUObject(this, &ThisClass::HandleMaxShieldChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetStaminaAttribute()).AddUObject(this, &ThisClass::HandleStaminaChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetMaxStaminaAttribute()).AddUObject(this, &ThisClass::HandleMaxStaminaChanged);
	
	CachedHealth = ASC->GetNumericAttribute(URogueVitalAttributeSet::GetHealthAttribute());
	CachedMaxHealth = ASC->GetNumericAttribute(URogueVitalAttributeSet::GetMaxHealthAttribute());
	CachedShield = ASC->GetNumericAttribute(URogueVitalAttributeSet::GetShieldAttribute());
	CachedMaxShield = ASC->GetNumericAttribute(URogueVitalAttributeSet::GetMaxShieldAttribute());
	CachedStamina = ASC->GetNumericAttribute(URogueVitalAttributeSet::GetStaminaAttribute());
	CachedMaxStamina = ASC->GetNumericAttribute(URogueVitalAttributeSet::GetMaxStaminaAttribute());
	
	// 초기에 0에서 1.0 까지 차오르는 걸 막기 위함
	// 초기 세팅에는 꽉 채워져 있어야함
	VitalBar->SetVitalsImmediate(MakeVitalValues());
	
	RefreshHealth();
	RefreshShield();

	Gauge_Stamina->SetProgressImmediate(CachedMaxStamina > 0.f ? CachedStamina / CachedMaxStamina : 0.f);
	RefreshStamina();
}

void UShooterHealthBar::OnAbilitySystemLost(URogueAbilitySystemComponent* ASC)
{
	ASC->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetHealthAttribute()).RemoveAll(this);
	ASC->GetGameplayAttributeValueChangeDelegate(
	URogueVitalAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
	ASC->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetShieldAttribute()).RemoveAll(this);
	ASC->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetMaxShieldAttribute()).RemoveAll(this);
	ASC->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetStaminaAttribute()).RemoveAll(this);
	ASC->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetMaxStaminaAttribute()).RemoveAll(this);
}

void UShooterHealthBar::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	// 전 체력보다 줄었으면 피격 받은걸로 판정
	if (Data.NewValue < Data.OldValue)
	{
		if (Anim_HealthHit)
		{
			PlayAnimation(Anim_HealthHit);
		}
		
		OnHealthDamaged.Broadcast();
	}
	
	CachedHealth = Data.NewValue;
	RefreshHealth();
}

void UShooterHealthBar::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	CachedMaxHealth = Data.NewValue;
	RefreshHealth();
}

void UShooterHealthBar::HandleShieldChanged(const FOnAttributeChangeData& Data)
{
	// 전 체력보다 줄었으면 피격 받은걸로 판정
	if (Data.NewValue < Data.OldValue)
	{
		if (Anim_ShieldHit)
		{
			PlayAnimation(Anim_ShieldHit);
		}
		
		OnShieldDamaged.Broadcast();
	}
	
	CachedShield = Data.NewValue;
	RefreshShield();
}

void UShooterHealthBar::HandleMaxShieldChanged(const FOnAttributeChangeData& Data)
{
	CachedMaxShield = Data.NewValue;
	RefreshShield();
}

void UShooterHealthBar::HandleStaminaChanged(const FOnAttributeChangeData& Data)
{
	CachedStamina = Data.NewValue;
	RefreshStamina();
}

void UShooterHealthBar::HandleMaxStaminaChanged(const FOnAttributeChangeData& Data)
{
	CachedMaxStamina = Data.NewValue;
	RefreshStamina();
}

void UShooterHealthBar::RefreshHealth()
{
	Text_Health->SetText(FText::AsNumber(CachedHealth));
	
	if (Text_MaxHealth)
	{
		Text_MaxHealth->SetText(FText::AsNumber(CachedMaxHealth));
	}
	if (Text_Health_Ghost)
	{
		Text_Health_Ghost->SetText(FText::AsNumber(CachedHealth));
	}
	if (Text_MaxHealth_Ghost)
	{
		Text_MaxHealth_Ghost->SetText(FText::AsNumber(CachedMaxHealth));
	}
	
	const float TargetHealthRatio = (CachedMaxHealth > 0.f) ? CachedHealth / CachedMaxHealth : 0.f;
	
	VitalBar->SetVitals(MakeVitalValues());
	
	// 체력이 LowHealthThreshold 아래로 내려가거나 올라가면 Broadcast
	const bool bNowLow = (TargetHealthRatio <= LowHealthThreshold);
	if (bNowLow != bIsLowHealth)
	{
		bIsLowHealth = bNowLow;
		OnLowHealthStateChanged.Broadcast(bNowLow);
	}
}

void UShooterHealthBar::RefreshShield()
{
	Text_Shield->SetText(FText::AsNumber(CachedShield));
	Text_Shield->SetVisibility(CachedShield > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	VitalBar->SetVitals(MakeVitalValues());

	if (Image_ShieldDeco)
	{
		Image_ShieldDeco->SetVisibility(CachedShield > 0.f ?ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UShooterHealthBar::RefreshStamina()
{
	Gauge_Stamina->SetProgress(CachedMaxStamina > 0.f ? CachedStamina / CachedMaxStamina : 0.f);

	// 콜백이 0.1초마다 오므로 상태가 바뀔 때만 튼다
	const bool bNowShown = (CachedStamina < CachedMaxStamina);
	if (bNowShown == bStaminaShown)
	{
		return;
	}

	bStaminaShown = bNowShown;

	if (bNowShown)
	{
		PlayAnimationForward(Anim_StaminaShow);
	}
	else
	{
		PlayAnimationReverse(Anim_StaminaShow);
	}
}

FShooterVitalValues UShooterHealthBar::MakeVitalValues() const
{
	return FShooterVitalValues(CachedHealth, CachedMaxHealth, CachedShield, CachedMaxShield);
}

bool UShooterHealthBar::IsLowHealth() const
{
	return bIsLowHealth;
}
