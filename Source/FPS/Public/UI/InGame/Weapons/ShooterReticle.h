// ShooterReticle.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GAS/Damage/RogueDamageTypes.h"
#include "Blueprint/UserWidget.h"
#include "ShooterReticle.generated.h"

class AWeaponBase;
class UImage;
class UWidgetAnimation;

/**
 * 크로스헤어 탄퍼짐(Spread)만 표현한다.
 * GetSpreadDegrees()가 장착 무기의 AWeaponBase::GetCurrentSpreadDegrees()를 읽는다.
 */
UCLASS(Abstract)
class FPS_API UShooterReticle : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "UI")
	float ComputeSpreadRadius() const;
	
	// 탄퍼짐이 없는 상태인지 확인한다.
	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsPerfectAccuracy() const;

	// 현재 장착 무기가 ADS 상태인지 UI에 전달한다.
	UFUNCTION(BlueprintPure, Category = "UI|Reticle")
	bool IsAiming() const;

	// 현재 장착 무기의 태그를 UI에 전달한다.
	UFUNCTION(BlueprintPure, Category = "UI|Reticle")
	FGameplayTag GetEquippedWeaponTag() const;

	// 히트마커 표시 중인지 UI에 전달한다.
	// 기본 크로스헤어 Visibility는 Blueprint의 단일 바인딩에서 결정한다.
	UFUNCTION(BlueprintPure, Category = "UI|Reticle")
	bool IsHitMarkerActive() const;

	// 마지막 히트마커의 부위(바디/쉴드/헤드샷). Blueprint가 색상 외에 아이콘까지 바꾸고 싶을 때 사용한다.
	UFUNCTION(BlueprintPure, Category = "UI|Reticle")
	ERogueHitMarkerType GetHitMarkerType() const { return LastHitMarkerType; }

	// 마지막 히트마커가 처치였는지 UI에 전달한다.
	UFUNCTION(BlueprintPure, Category = "UI|Reticle")
	bool WasHitMarkerKill() const { return bLastHitMarkerKill; }

	// 중앙 킬 이펙트(해골 등) 표시 중인지 UI에 전달한다.
	UFUNCTION(BlueprintPure, Category = "UI|Reticle")
	bool IsKillEffectActive() const { return bKillEffectActive; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	// 장착 무기가 없거나 동적 크로스헤어가 꺼져 있으면 0.
	float GetSpreadDegrees() const;

	AWeaponBase* FindEquippedWeapon() const;

	// 히트마커 자체의 표시 상태만 관리한다.
	// 기본 크로스헤어의 Visibility는 Blueprint 바인딩이 전담한다.
	void HandleHitMarker(ERogueHitMarkerType Type, bool bKill);
	void HideHitMarker();

	// Type/bKill로부터 히트마커 RGB를 결정한다(알파 제외). 처치는 부위와 무관하게 빨간색으로 강조한다.
	// 알파는 HitMarkerOpacity로 따로 곱해서, 크로스헤어와 겹쳐도 뒤가 비쳐 보이게 한다.
	static FLinearColor ResolveHitMarkerColor(ERogueHitMarkerType Type, bool bKill);

	UFUNCTION()
	void HandleHitMarkerAnimationFinished();

	// 중앙 킬 이펙트(해골 등)의 표시 상태만 관리한다. 크기 변화(크게 등장 -> 작아지며 페이드)는
	// Anim_KillEffect의 RenderTransform Scale + Render Opacity 트랙으로 WBP에서 만든다.
	void HandleKillEffect();
	void HideKillEffect();

	UFUNCTION()
	void HandleKillEffectAnimationFinished();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Top;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Bottom;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Left;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Right;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Center;

	// 크로스헤어와 같은 위치(중앙)에 겹쳐서 뜨는 히트마커. HitMarkerOpacity로 반투명 처리해서
	// 겹쳐도 크로스헤어가 뒤로 비쳐 보이게 한다(오버워치 스타일).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_HitMarker;

	// 히트마커의 최대 불투명도. 1.0이면 완전 불투명(크로스헤어를 완전히 가림),
	// 낮출수록 크로스헤어가 더 많이 비쳐 보인다.
	UPROPERTY(EditAnywhere, Category = "UI", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HitMarkerOpacity = 0.75f;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_HitMarker;

	// 크로스헤어/히트마커와 같은 중앙에 겹쳐서 뜨는 킬 이펙트(해골 등). 크게 등장했다가 작아지며
	// 페이드 아웃되는 연출은 Anim_KillEffect의 RenderTransform Scale + Render Opacity 트랙이 담당한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_KillEffect;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_KillEffect;

	// 킬 이펙트의 최대 불투명도. 크로스헤어/히트마커가 같이 보이도록 1.0보다 낮게 잡는다.
	UPROPERTY(EditAnywhere, Category = "UI", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float KillEffectOpacity = 0.85f;

	// Anim_KillEffect가 없을 때 킬 이펙트를 표시하는 시간
	UPROPERTY(EditAnywhere, Category = "UI")
	float KillEffectDuration = 0.6f;

	// 탄퍼짐 0일때 간격
	UPROPERTY(EditAnywhere, Category = "UI")
	float MinSpreadRadius = 8.f;

	// 탄퍼짐 표현 정도가 심해지면 제한한다
	UPROPERTY(EditAnywhere, Category = "UI")
	float SpreadRadiusScale = 1.f;

	// Anim_HitMarker가 없을 때 히트마커를 표시하는 시간
	UPROPERTY(EditAnywhere, Category = "UI")
	float HitMarkerDuration = 0.4f;

private:
	FTimerHandle HitMarkerTimerHandle;
	bool bHitMarkerActive = false;
	ERogueHitMarkerType LastHitMarkerType = ERogueHitMarkerType::Body;
	bool bLastHitMarkerKill = false;

	FTimerHandle KillEffectTimerHandle;
	bool bKillEffectActive = false;
};
