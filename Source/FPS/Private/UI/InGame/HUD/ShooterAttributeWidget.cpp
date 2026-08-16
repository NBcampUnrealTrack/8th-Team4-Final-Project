// ShooterAttributeWidget.cpp
#include "UI/InGame/HUD/ShooterAttributeWidget.h"
#include "Player/ShooterPlayerState.h"
#include "GAS/RogueAbilitySystemComponent.h"

void UShooterAttributeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	TryBindToAbilitySystem();
}

void UShooterAttributeWidget::NativeDestruct()
{
	if (BoundASC.IsValid())
	{
		OnAbilitySystemLost(BoundASC.Get());
		BoundASC.Reset();
	}
	
	Super::NativeDestruct();
}

bool UShooterAttributeWidget::TryBindToAbilitySystem()
{
	AShooterPlayerState* PS = GetOwningPlayerState<AShooterPlayerState>();
	if (IsValid(PS) == false)
	{
		return false;
	}
	
	URogueAbilitySystemComponent* ASC = PS->GetRogueAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return false;
	}
	
	BoundASC = ASC;
	OnAbilitySystemReady(ASC);
	
	return true;
}


