// ShooterHealthBar.h
#pragma once

#include "CoreMinimal.h"
#include "ShooterAttributeWidget.h"
#include "ShooterHealthBar.generated.h"

class UShooterGauge;
class UCommonTextBlock;
class UImage;
class UMaterialInstanceDynamic;
class URogueAbilitySystemComponent;
class UWidgetAnimation;
struct FOnAttributeChangeData;

// HUD가 구독해서 피격시 화면 전체 Feedback 연출 재생
DECLARE_MULTICAST_DELEGATE(FOnHealthDamaged);
// Shield가 있을때 피격시 화면 전체 Feedback 연출 재생
DECLARE_MULTICAST_DELEGATE(FOnShieldDamaged);
// 저체력 진입 / 이탈 상태 변경 시 전체 화면 글리치 발생
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLowHealthStateChanged, bool /*bIsLow*/);

/**
 * 체력 / 보호막 표시하는 HUD의 자식
 * 체력 델리게이트가 없으므로 ASC 직접 구독 
 */
UCLASS()
class FPS_API UShooterHealthBar : public UShooterAttributeWidget
{
	GENERATED_BODY()
	
public:
	FOnHealthDamaged OnHealthDamaged;
	FOnShieldDamaged OnShieldDamaged;
	FOnLowHealthStateChanged OnLowHealthStateChanged;
	
	// HUD가 구독전에 이미 발사됐으면 델리게이트를 놓치기때문에 구독 직후 이걸로 확인
	bool IsLowHealth() const;
	
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	
	// Attribute Delegate 설정
	void OnAbilitySystemReady(URogueAbilitySystemComponent* ASC);
	// Delegate 해제 
	void OnAbilitySystemLost(URogueAbilitySystemComponent* ASC);
	
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
	void HandleShieldChanged(const FOnAttributeChangeData& Data);
	void HandleMaxShieldChanged(const FOnAttributeChangeData& Data);
	
	// 캐싱 된 값으로 텍스트 갱신 후 목표 비율을 다시 계산한다.
	void RefreshHealth();
	void RefreshShield();
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Health;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_MaxHealth;
	// Text 연출용
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_Health_Ghost;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_MaxHealth_Ghost;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Shield;
	// WBP_ShooterGauge 
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterGauge> Gauge_Health;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterGauge> Gauge_Shield;
	
	// 체력 바 자체 피격 연출
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_HealthHit;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_ShieldHit;
	
	// 이 비율 이하면 저체력으로 본다
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	float LowHealthThreshold = 0.3f;
	
	// 마지막으로 받은 값
	float CachedHealth = 0.f;
	float CachedMaxHealth = 0.f; 
	float CachedShield = 0.f;
	float CachedMaxShield = 0.f; 
	
	bool bIsLowHealth = false;
};
