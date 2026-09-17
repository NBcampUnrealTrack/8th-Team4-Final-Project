// PingIndicatorEntry.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Ping/PingTypes.h"
#include "PingIndicatorEntry.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class UPanelWidget;

/**
 * 핑 하나를 화면에 표시하는 월드공간 인디케이터. TeammateIndicatorEntry와 같은 투영 방식을
 * 쓰지만 대상이 움직이는 액터가 아니라 고정된 월드 좌표이고, 일정 시간 뒤 스스로 만료된다.
 * UPingIndicatorLayer가 풀링해서 재사용한다.
 */
UCLASS()
class FPS_API UPingIndicatorEntry : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	// Layer가 생성 직후 1회 호출
	void InitializeIndicator(UCanvasPanel* InOwnerCanvas, UCanvasPanelSlot* InCanvasSlot);

	// 새 핑으로 활성화. 이미 활성 상태였어도 새 핑으로 덮어쓰고 타이머를 재시작한다.
	void Activate(const FPingEvent& Ping);

	bool IsActive() const { return bActive; }

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void UpdateScreenPosition();
	void Deactivate();

	UFUNCTION()
	void HandleExpired();

	// 화면 밖/투영 실패일 때 이 패널만 Collapsed
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Panel_IndicatorVisual;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_LocationIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_EnemyIcon;

private:
	FVector PingLocation = FVector::ZeroVector;

	// 매프레임 GetParent() 캐스팅 피하기 위함 캐싱
	TWeakObjectPtr<UCanvasPanel> OwnerCanvas;
	TWeakObjectPtr<UCanvasPanelSlot> CachedCanvasSlot;

	FTimerHandle ExpiryTimerHandle;
	bool bActive = false;
};
