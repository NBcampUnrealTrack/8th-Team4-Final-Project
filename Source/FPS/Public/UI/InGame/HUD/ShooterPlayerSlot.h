// ShooterPlayerSlot.h
#pragma once

#include "CoreMinimal.h"
#include "ShooterAttributeWidget.h"
#include "ShooterPlayerSlot.generated.h"

class AShooterPlayerState;
class UImage;
class UShooterGauge;

/**
 * 팀 상태 표시의 최소 단위인 플레이어 한 명
 * 아군/적군 공용으로 둔다. 둘의 차이는 HP 게이지와 표시 색 뿐이라 SetSlotStyle()로 다르게 표현
 * 명단 관리(팀 필터, PlayerArray  훑기)는 UShooterTeamStatus가 한다
 */
UCLASS()
class FPS_API UShooterPlayerSlot : public UShooterAttributeWidget
{
	GENERATED_BODY()
	
public:
	UShooterPlayerSlot();
	
	// 컨테이너가 호출. nullptr이면 슬롯을 비움
	void SetPlayerState(AShooterPlayerState* NewPlayerState);
	
	// 아군/적군 외형 결정. SetPlayerState보다 선호출
	void SetSlotStyle(bool bInShowHealth, FLinearColor InTintColor);

protected:
	virtual void NativeDestruct() override;
	
	virtual void OnAbilitySystemReady(URogueAbilitySystemComponent* ASC) override;
	virtual void OnAbilitySystemLost(URogueAbilitySystemComponent* ASC) override;
	
	void UnbindFromPlayerState();
	
	void RefreshHealth();

	UFUNCTION()
	void HandleAliveStateChanged(bool bNewIsAlive);

	void HandleHealthChanged(const struct FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const struct FOnAttributeChangeData& Data);
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_AliveIcon;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_DeadIcon;
	// 적군 슬롯에는 없음
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UShooterGauge> Gauge_Health;
	
private:
	TWeakObjectPtr<AShooterPlayerState> BoundPlayerState;
	
	bool bShowHealth = false;
	FLinearColor TintColor = FLinearColor::White;
	
	float CachedHealth = 0.f;
	float CachedMaxHealth = 0.f;
};
