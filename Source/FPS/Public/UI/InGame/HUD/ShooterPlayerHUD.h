// ShooterPlayerHUD.h
#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterPlayerHUD.generated.h"

class APawn;
class APlayerController;
class UImage;
class UCombatComponent;
class UShooterHealthBar;
class UWidgetAnimation;
class URetainerBox;
class UMaterialInstanceDynamic;

/**
 * UI.Layer.Game에 올라가는 인게임 HUD 레이아웃
 * 
 * 패널들은 각자 담당 위젯이 채운다. 예를들어
 * Panel_GameFlow - WBP_BombGameFlow가 담당
 * Panel_Status - WBP_ShooterHealthBar
 * Panel_Weapon - WBP_ShooterWeaponStatus
 */
UCLASS(meta = (DisableNativeTick))
class FPS_API UShooterPlayerHUD : public UShooterActivatableWidget
{
	GENERATED_BODY()

public:
	UShooterPlayerHUD();
	
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	// 랜덤 시간마다 글리치
	void ScheduleNextLowHealthGlitch();
	// 글리치 1회 재생 후 다음 랜덤 글리치 타이머 예약 
	void PlayLowHealthGlitchPulse();
	
	void HandleHealthDamaged();
	void HandleShieldDamaged();
	void HandleLowHealthStateChanged(bool bIsLow);

	// 로컬 Pawn의 CombatComponent가 준비되면 ADS HUD 표현 상태를 구독한다.
	void BindToPawn(APawn* Pawn);
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);
	void UnbindCombatComponent();
	void ApplyMainHUDSuppression(bool bSuppressed);

	UFUNCTION()
	void HandleMainHUDSuppressionChanged(bool bSuppressed);
	
	// LowHealth 애니메이션이 끝나면 HUD 글리치도 같이 종료
	UFUNCTION()
	void HandleLowHealthAnimationFinished();
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterHealthBar> HealthBar;
	
	// HUD 글리치를 위한 RetainerBox 
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<URetainerBox> RetainerBox_HUD;
	
	// 런타임 파라미터 제어에 사용
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HUDGlitchMID;
	
	// 체력 피격 순간 붉은 글리치 비네트
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_HitFlash_Health;

	// 보호막 피격 순간 시안 글리치 비네트
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_HitFlash_Shield;

	// 비네트 효과
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_LowHealthVignette;
	
	// 저체력 상태 동안 지속되는 비네트
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_LowHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Glitch")
	float RandomGlitchMin = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Glitch")
	float RandomGlitchMax = 4.0f;
	
	FTimerHandle LowHealthGlitchTimerHandle;
	TWeakObjectPtr<APlayerController> BoundPlayerController;
	TWeakObjectPtr<UCombatComponent> BoundCombatComponent;
	ESlateVisibility MainHUDContentDefaultVisibility = ESlateVisibility::Visible;
	bool bMainHUDDefaultVisibilityCached = false;
	bool bIsLowHealthActive = false;
	
};
