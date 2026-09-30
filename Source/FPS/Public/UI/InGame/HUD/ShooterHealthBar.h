// ShooterHealthBar.h
#pragma once

#include "CoreMinimal.h"
#include "ShooterAttributeWidget.h"
#include "ShooterHealthBar.generated.h"

class UShooterVitalBar;
class UShooterGauge;
class UCommonTextBlock;
class UImage;
class UMaterialInstanceDynamic;
class URogueAbilitySystemComponent;
class UWidgetAnimation;
struct FOnAttributeChangeData;
struct FShooterVitalValues;

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
UCLASS(meta = (DisableNativeTick))
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
	void HandleStaminaChanged(const FOnAttributeChangeData& Data);
	void HandleMaxStaminaChanged(const FOnAttributeChangeData& Data);
	
	// 캐싱 된 값으로 텍스트 갱신 후 바에 전달한다.
	void RefreshHealth();
	void RefreshShield();
	// 게이지 갱신, 가득 참 여부가 바뀔 때만 페이드
	void RefreshStamina();

	// 캐시 4개로 만든 바 입력
	FShooterVitalValues MakeVitalValues() const;
	
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
	// WBP_ShooterVitalBar
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterVitalBar> VitalBar;
	// 쉴드 용량이 있을 때만 보이는 장식
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_ShieldDeco;
	// WBP_ShooterGauge
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterGauge> Gauge_Stamina;
	
	// 체력 바 자체 피격 연출
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_HealthHit;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_ShieldHit;
	// Gauge_Stamina 불투명도 0 → 1. 가득 차면 역재생
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> Anim_StaminaShow;
	
	// 이 비율 이하면 저체력으로 본다
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	float LowHealthThreshold = 0.3f;
	
	// 마지막으로 받은 값
	float CachedHealth = 0.f;
	float CachedMaxHealth = 0.f; 
	float CachedShield = 0.f;
	float CachedMaxShield = 0.f; 
	float CachedStamina = 0.f;
	float CachedMaxStamina = 0.f;
	
	bool bIsLowHealth = false;
	bool bStaminaShown = false;
};
