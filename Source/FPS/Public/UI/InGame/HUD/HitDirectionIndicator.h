// HitDirectionIndicator.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "HitDirectionIndicator.generated.h"

class AShooterPlayerState;
class UImage;
class UWidgetAnimation;
class URogueHealthComponent;

/**
 * 크로스헤어 중심 피격 방향 인디케이터.
 * 로컬 플레이어 PlayerState의 URogueHealthComponent::OnHitReactionReceived(COND_OwnerOnly로 복제되어
 * 본인 피격시에만 발신)를 구독해, 피격 방향으로 Image_HitDirection을 회전시키고 Anim_HitFade를 재생한다.
 */
UCLASS(Abstract)
class FPS_API UHitDirectionIndicator : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// PlayerState/HealthComponent가 아직 준비되지 않았을 수 있어 재시도한다
	// (ShooterPlayerHUD::TryBindCombatComponent와 동일 패턴).
	void TryBindHealthComponent();
	void UnbindHealthComponent();

	UFUNCTION()
	void HandleHitReactionReceived(FVector WorldDirection);

	// 정면 위쪽을 가리키는 형태로 제작된 빨간 아크/쐐기 한 장. 피격 방향으로 회전시켜 표시한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_HitDirection;

	// 오파시티 0->1->0 페이드 인/아웃 애니메이션.
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_HitFade;

private:
	TWeakObjectPtr<URogueHealthComponent> BoundHealthComponent;
	FTimerHandle HealthBindingRetryTimerHandle;
};
