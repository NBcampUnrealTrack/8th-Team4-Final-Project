// ShooterPlayerHUD.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "ShooterPlayerHUD.generated.h"

class ACharacter;
class APawn;
class FActiveTimerHandle;
enum class EActiveTimerReturnType : uint8;
class APlayerController;
class UImage;
class UCombatComponent;
class UShooterHealthBar;
class UWidgetAnimation;
class URetainerBox;
class UMaterialInstanceDynamic;
class UWidget;

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

	// 점프·착지 때 Panel_HUDMotion 을 위아래로 튕긴다
	void BindMovement(APawn* Pawn);
	void UnbindMovement();
	UFUNCTION()
	void HandleMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);
	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);
	void EnsureMotionScheduled();
	void StopMotion();
	void ResetMotion();
	EActiveTimerReturnType HandleMotionTimer(double InCurrentTime, float InDeltaTime);

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

	// 가장자리 패널만 담는다. 크로스헤어·월드 마커·전체화면 비네트는 밖에 둔다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_HUDMotion;

	// 점프 시작 높이보다 1cm 올라갈 때마다 위로 뜨는 거리
	UPROPERTY(EditAnywhere, Category = "HUD|Motion", meta = (ClampMin = "0.0"))
	float AirLiftPerCm = 0.12f;
	// 공중에서 가장 높이 뜨는 거리. 증강으로 점프가 높아져도 여기서 멈춘다
	UPROPERTY(EditAnywhere, Category = "HUD|Motion", meta = (ClampMin = "0.0"))
	float MaxAirLift = 10.f;
	// 가장 세게 착지했을 때 아래로 차는 속도 
	UPROPERTY(EditAnywhere, Category = "HUD|Motion", meta = (ClampMin = "0.0"))
	float LandingKickSpeed = 400.f;
	// 이 낙하 속도 이하면 착지 반동 없음. 카메라 착지 셰이크와 같은 기준
	UPROPERTY(EditAnywhere, Category = "HUD|Motion", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinLandingSpeed = 180.f;
	// 최대 반동 낙하 속도
	UPROPERTY(EditAnywhere, Category = "HUD|Motion", meta = (ClampMin = "1.0", Units = "cm/s"))
	float FullLandingSpeed = 600.f;
	// 출렁이는 빠르기
	UPROPERTY(EditAnywhere, Category = "HUD|Motion", meta = (ClampMin = "0.1"))
	float MotionFrequency = 4.f;
	// 착지 반동에만 쓴다. 1 이면 출렁임 없이 멈추고 낮을수록 더 튄다
	UPROPERTY(EditAnywhere, Category = "HUD|Motion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MotionDampingRatio = 0.45f;

	FTimerHandle LowHealthGlitchTimerHandle;
	TWeakObjectPtr<APlayerController> BoundPlayerController;
	TWeakObjectPtr<UCombatComponent> BoundCombatComponent;
	TWeakObjectPtr<ACharacter> BoundCharacter;
	TWeakPtr<FActiveTimerHandle> MotionTimerHandle;
	float MotionOffset = 0.f;
	float MotionVelocity = 0.f;
	float TakeoffZ = 0.f;
	ESlateVisibility MainHUDContentDefaultVisibility = ESlateVisibility::Visible;
	bool bMainHUDDefaultVisibilityCached = false;
	bool bIsLowHealthActive = false;
	
};
