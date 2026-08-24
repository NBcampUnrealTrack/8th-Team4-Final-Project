// ShooterAttributeWidget.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ShooterAttributeWidget.generated.h"

class AShooterPlayerState;
class URogueAbilitySystemComponent;

/**
 * ASC 파이프라인
 * ShooterUIInitComponent에 PlayerState 대기 후 HUD를 Push하므로
 * 이 위젯이 생성되는 시점엔 ASC 항상 존재
 * 
 * 남의 ASC를 봐야 하는 위젯은 bAutoBindToOwningPlayer = false로 두고
 * SetSourcePlayerState()로 대상을 지정한다. 
 */
UCLASS()
class FPS_API UShooterAttributeWidget : public UCommonUserWidget
{
	GENERATED_BODY()
	
public:
	// 바인딩 대상 지정, 이미 다른 대상에 붙어 있으면 해제 후 재바인딩
	UFUNCTION(Blueprintable, Category = "Shooter|UI")
	void SetSourcePlayerState(AShooterPlayerState* NewSource);
	
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	
	// ASC를 찾아 성공하면 true 반환 함수
	bool TryBindToAbilitySystem();
	
	// 현재 붙어 있는 ASC 해제. 재바인딩 전에 항상 먼저 호출
	void UnbindFromAbilitySystem();
	
	virtual void OnAbilitySystemReady(URogueAbilitySystemComponent* ASC);
	virtual void OnAbilitySystemLost(URogueAbilitySystemComponent* ASC);
	
	// false면 NativeOnInitialized에서 자동 바인딩 하지 않음
	// 대상을 나중에 받는 Widget은 false로 둔다.
	UPROPERTY(EditDefaultsOnly, Category = "Shooter|UI")
	bool bAutoBindToOwningPlayer = true;
	
	TWeakObjectPtr<URogueAbilitySystemComponent> BoundASC;
	
private:
	// 위젯에서 명시 지정된 대상으로 비어있으면 GetOwningPlayerState로 폴백
	TWeakObjectPtr<AShooterPlayerState> SourcePlayerState;
};
