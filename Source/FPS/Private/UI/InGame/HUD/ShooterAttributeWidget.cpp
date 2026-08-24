// ShooterAttributeWidget.cpp
#include "UI/InGame/HUD/ShooterAttributeWidget.h"
#include "Player/ShooterPlayerState.h"
#include "GAS/RogueAbilitySystemComponent.h"

void UShooterAttributeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	if (bAutoBindToOwningPlayer)
	{
		TryBindToAbilitySystem();
	}
}

void UShooterAttributeWidget::NativeDestruct()
{
	UnbindFromAbilitySystem();
	
	Super::NativeDestruct();
}

bool UShooterAttributeWidget::TryBindToAbilitySystem()
{
	AShooterPlayerState* PS = SourcePlayerState.IsValid() ? SourcePlayerState.Get() : GetOwningPlayerState<AShooterPlayerState>();
	if (IsValid(PS) == false)
	{
		return false;
	}
	
	URogueAbilitySystemComponent* ASC = PS->GetRogueAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return false;
	}
	
	// 실제로 붙은 대상 기록 폴백으로 찾은 경우에도 찾아놔야함
	SourcePlayerState = PS;
	
	BoundASC = ASC;
	OnAbilitySystemReady(ASC);
	
	return true;
}

void UShooterAttributeWidget::UnbindFromAbilitySystem()
{
	if (BoundASC.IsValid())
	{
		OnAbilitySystemLost(BoundASC.Get());
	}
	
	BoundASC.Reset();
}

void UShooterAttributeWidget::SetSourcePlayerState(AShooterPlayerState* NewSource)
{
	// 같은 대상이면 아무것도 하지 않음
	if (SourcePlayerState.Get() == NewSource)
	{
		return;
	}
	
	// 새 대상을 넣기 전에 이전 ASC 해제
	UnbindFromAbilitySystem();
	
	SourcePlayerState = NewSource;
	
	if (NewSource)
	{
		TryBindToAbilitySystem();
	}
}

void UShooterAttributeWidget::OnAbilitySystemReady(URogueAbilitySystemComponent* ASC)
{
}

void UShooterAttributeWidget::OnAbilitySystemLost(URogueAbilitySystemComponent* ASC)
{
}
