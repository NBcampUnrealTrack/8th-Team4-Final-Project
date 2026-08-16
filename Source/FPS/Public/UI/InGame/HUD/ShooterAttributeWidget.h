// ShooterAttributeWidget.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ShooterAttributeWidget.generated.h"

class URogueAbilitySystemComponent;

/**
 * ASC 파이프라인
 * ShooterUIInitComponent에 PlayerState 대기 후 HUD를 Push하므로
 * 이 위젯이 생성되는 시점엔 ASC 항상 존재한다. 
 * 위와 같은 이유로 재시도 로직은 없음
 */
UCLASS()
class FPS_API UShooterAttributeWidget : public UCommonUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	
	// ASC를 찾아 성공하면 true 반환 함수
	bool TryBindToAbilitySystem();
	
	virtual void OnAbilitySystemReady(URogueAbilitySystemComponent* ASC) override;
	virtual void OnAbilitySystemLost(URogueAbilitySystemComponent* ASC) override;
	
	TWeakObjectPtr<URogueAbilitySystemComponent> BoundASC;
	
};
