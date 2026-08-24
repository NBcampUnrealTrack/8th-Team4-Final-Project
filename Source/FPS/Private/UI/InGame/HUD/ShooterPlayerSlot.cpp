// ShooterPlayerSlot.cpp
#include "UI/InGame/HUD/ShooterPlayerSlot.h"

#include "Components/Image.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Player/ShooterPlayerState.h"
#include "UI/Common/ShooterGauge.h"

UShooterPlayerSlot::UShooterPlayerSlot()
{
	// 대상을 컨테이너가 나중에 지정. 로컬 플레이어에 자동을 붙기 방지
	bAutoBindToOwningPlayer = false;
}

void UShooterPlayerSlot::NativeDestruct()
{
	UnbindFromPlayerState();
	
	Super::NativeDestruct();
}

void UShooterPlayerSlot::OnAbilitySystemReady(URogueAbilitySystemComponent* ASC)
{
	Super::OnAbilitySystemReady(ASC);
	
	if (bShowHealth == false)
	{
		return;
	}
	
	ASC->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HandleMaxHealthChanged);
	
	CachedHealth = ASC->GetNumericAttribute(URogueVitalAttributeSet::GetHealthAttribute());
	CachedMaxHealth = ASC->GetNumericAttribute(URogueVitalAttributeSet::GetMaxHealthAttribute());

	const float TargetHealthRatio = (CachedMaxHealth > 0.f) ? CachedHealth / CachedMaxHealth : 0.f;
	
	Gauge_Health->SetProgressImmediate(TargetHealthRatio);
}

void UShooterPlayerSlot::OnAbilitySystemLost(URogueAbilitySystemComponent* ASC)
{
	Super::OnAbilitySystemLost(ASC);
	
	ASC->GetGameplayAttributeValueChangeDelegate(
	URogueVitalAttributeSet::GetHealthAttribute()).RemoveAll(this);
	ASC->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
}

void UShooterPlayerSlot::UnbindFromPlayerState()
{
	if (BoundPlayerState.IsValid())
	{
		BoundPlayerState->OnPlayerAliveStateChanged.RemoveAll(this);
	}
	
	BoundPlayerState.Reset();
}

void UShooterPlayerSlot::SetPlayerState(AShooterPlayerState* NewPlayerState)
{
	UnbindFromPlayerState();
	
	BoundPlayerState = NewPlayerState;
	
	// 비우기 요청이면 숨기고 끝낸다.
	if (NewPlayerState == nullptr)
	{
		SetSourcePlayerState(nullptr);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	
	NewPlayerState->OnPlayerAliveStateChanged.AddDynamic(this, &ThisClass::HandleAliveStateChanged);
	
	// 현재 생사 즉시 반영
	HandleAliveStateChanged(NewPlayerState->IsAlive());
	
	// ShooterAttributeWidget에서 구독
	SetSourcePlayerState(NewPlayerState);
}

void UShooterPlayerSlot::SetSlotStyle(bool bInShowHealth, FLinearColor InTintColor)
{
	bShowHealth = bInShowHealth;
	TintColor = InTintColor;
	
	if (Image_AliveIcon)
	{
		Image_AliveIcon->SetColorAndOpacity(TintColor);
	}
	
	if (Image_DeadIcon)
	{
		Image_DeadIcon->SetColorAndOpacity(TintColor);
	}
	
	if (Gauge_Health)
	{
		Gauge_Health->SetVisibility(bShowHealth ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UShooterPlayerSlot::RefreshHealth()
{
	if (CachedMaxHealth > 0.f && Gauge_Health)
	{
		const float TargetHealthRatio = (CachedMaxHealth > 0.f) ? CachedHealth / CachedMaxHealth : 0.f;

		Gauge_Health->SetProgress(TargetHealthRatio);
	}
}

void UShooterPlayerSlot::HandleAliveStateChanged(bool bNewIsAlive)
{
	if (Image_AliveIcon && Image_DeadIcon)
	{
		Image_AliveIcon->SetVisibility(bNewIsAlive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
		
		Image_DeadIcon->SetVisibility(bNewIsAlive ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UShooterPlayerSlot::HandleHealthChanged(const struct FOnAttributeChangeData& Data)
{
	CachedHealth = Data.NewValue;
	RefreshHealth();
}

void UShooterPlayerSlot::HandleMaxHealthChanged(const struct FOnAttributeChangeData& Data)
{
	CachedMaxHealth = Data.NewValue;
	RefreshHealth();
}




