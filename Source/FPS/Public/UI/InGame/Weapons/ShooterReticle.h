// ShooterReticle.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GAS/Damage/RogueDamageTypes.h"
#include "Blueprint/UserWidget.h"
#include "ShooterReticle.generated.h"

class AWeaponBase;
class APawn;
class FViewport;
class UCombatComponent;
class UImage;
class UWidgetAnimation;

/**
 * 크로스헤어 탄퍼짐(Spread)만 표현한다.
 * GetSpreadDegrees()가 장착 무기의 AWeaponBase::GetCurrentSpreadDegrees()를 읽는다.
 * 폴링하지 않는다 - CombatComponent::OnCrosshairSpreadInputsChanged(무기/탄퍼짐/FOV 변경),
 * 크로스헤어 설정 변경, 뷰포트 크기 변경 시점에만 4방향 간격을 다시 계산한다.
 */
UCLASS(Abstract, meta = (DisableNativeTick))
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
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 빙의 대상(리스폰 포함)이 바뀌면 새 Pawn의 CombatComponent로 구독을 옮긴다.
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	// 이전 CombatComponent 구독을 해제하고 새 CombatComponent(nullptr 가능)를 구독한다.
	void BindToCombatComponent(UCombatComponent* NewCombat);

	// 현재 탄퍼짐/FOV/뷰포트 기준으로 4방향 크로스헤어 간격을 다시 계산해 적용한다.
	void RefreshSpreadOffsets();

	void HandleViewportResized(FViewport* Viewport, uint32 Unused);

	// 장착 무기가 없거나 동적 크로스헤어가 꺼져 있으면 0.
	float GetSpreadDegrees() const;

	// 화면에 보이는 Pawn. 평소엔 내 Pawn, 관전 중엔 관전 대상이다.
	APawn* GetViewedPawn() const;

	AWeaponBase* FindEquippedWeapon() const;

	void PromoteSniperScopeToViewportLayer();
	void RemoveSniperScopeFromViewportLayer();

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

	// Designer에서는 기존처럼 ShooterReticle 안에 배치해도 된다. 런타임에는 같은 인스턴스를
	// 부모에서 분리해 독립 Viewport 레이어로 올려 Main HUD와 렌더 순서를 분리한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> WBP_SniperScope;

	// ShooterUIManagerSubsystem의 Root Layout(-1)보다 아래에 두는 기본값.
	// WBP_ShooterReticle Class Defaults에서 필요하면 값만 조정하면 된다.
	UPROPERTY(EditDefaultsOnly, Category = "UI|Scope")
	int32 SniperScopeViewportZOrder = -2;

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
	float KillEffectDuration = 0.3f;

	// 탄퍼짐 0일때 간격
	UPROPERTY(EditAnywhere, Category = "UI")
	float MinSpreadRadius = 8.f;

	// 탄퍼짐 표현 정도가 심해지면 제한한다
	UPROPERTY(EditAnywhere, Category = "UI")
	float SpreadRadiusScale = 1.f;

	// Top/Bottom 이미지가 Left/Right와 같은(회전된) 브러시를 쓸 때, 박스 크기 차이로 중심에서
	// 더 멀어 보이는 것을 보정한다. 1.0이면 Left/Right와 동일한 반경, 낮출수록 위/아래가 중앙에
	// 더 가깝게 붙는다.
	UPROPERTY(EditAnywhere, Category = "UI", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VerticalSpreadRadiusScale = 0.75f;

	// Top/Bottom 둘을 하나의 축으로 보고 한꺼번에 미세 이동시키는 오프셋(픽셀). Left/Right 기준선과
	// 안 맞을 때 여기서 X/Y를 같이 조절한다. (+X: 오른쪽, +Y: 아래쪽)
	// 주의: 0이 아닌 값을 주면 Top/Bottom 쌍의 시각적 중심이 (0,0)에서 벗어나는데, 히트마커/킬이펙트/
	// Image_Center는 여전히 화면 진짜 중앙인 (0,0)(ComputeSpreadRadius가 쓰는 뷰포트 중심과 동일)에
	// 고정되어 있어 서로 어긋나 보이게 된다. Top/Bottom 개별 보정이 필요하면 이 값 대신 TopBarLift처럼
	// 한쪽에만 적용되는 오프셋을 쓴다.
	UPROPERTY(EditAnywhere, Category = "UI")
	FVector2D VerticalGroupOffset = FVector2D::ZeroVector;

	// Left/Right 둘을 하나의 축으로 보고 한꺼번에 미세 이동시키는 오프셋(픽셀). (+X: 오른쪽, +Y: 아래쪽)
	// VerticalGroupOffset과 마찬가지로 0이 아닌 값을 주면 Left/Right 쌍의 시각적 중심이 화면 진짜
	// 중앙(0,0)에서 벗어나 히트마커/킬이펙트와 어긋나 보일 수 있다.
	UPROPERTY(EditAnywhere, Category = "UI")
	FVector2D HorizontalGroupOffset = FVector2D::ZeroVector;

	// 위쪽 막대(Image_Top)만 중심에서 더 멀리(위로) 밀어내는 추가 간격(픽셀). Image_Bottom에는 영향을
	// 주지 않으므로, 이 값을 조절해도 히트마커/킬이펙트가 기준으로 삼는 화면 진짜 중앙(0,0)은 그대로다.
	UPROPERTY(EditAnywhere, Category = "UI")
	float TopBarLift = 6.f;

	// Anim_HitMarker가 없을 때 히트마커를 표시하는 시간
	UPROPERTY(EditAnywhere, Category = "UI")
	float HitMarkerDuration = 0.4f;

private:
	// OnCrosshairSpreadInputsChanged 구독 해제용으로 현재 구독 중인 CombatComponent를 기억해둔다.
	TWeakObjectPtr<UCombatComponent> CachedCombat;
	FDelegateHandle CrosshairSpreadInputsHandle;
	FDelegateHandle CrosshairSettingsHandle;
	FDelegateHandle ViewportResizedHandle;

	FTimerHandle HitMarkerTimerHandle;
	bool bHitMarkerActive = false;
	ERogueHitMarkerType LastHitMarkerType = ERogueHitMarkerType::Body;
	bool bLastHitMarkerKill = false;

	FTimerHandle KillEffectTimerHandle;
	bool bKillEffectActive = false;
};
