// PingIndicatorLayer.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Ping/PingTypes.h"
#include "PingIndicatorLayer.generated.h"

class AShooterPlayerController;
class UCanvasPanel;
class UPingIndicatorEntry;

/**
 * 핑 인디케이터 컨테이너. TeammateIndicatorLayer와 달리 로스터 기반이 아니라
 * AShooterPlayerController::OnPingReceived 이벤트가 올 때마다 빈 Entry를 하나 꺼내
 * 활성화하는 구조다 - Entry는 자기가 알아서 만료(ShooterPingLimits::PingLifetimeSeconds)되므로
 * 레이어는 재사용 가능한 Entry를 찾아 돌려주기만 한다.
 */
UCLASS()
class FPS_API UPingIndicatorLayer : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandlePingReceived(const FPingEvent& Ping);

	// 비활성 Entry가 있으면 재사용하고, 없으면 새로 만든다.
	UPingIndicatorEntry* GetFreeEntry();

	UPROPERTY(EditDefaultsOnly, Category = "Shooter|UI")
	TSubclassOf<UPingIndicatorEntry> EntryWidgetClass;

	// Entry는 이 Canvas의 자식이여야 함.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> Box_Indicators;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPingIndicatorEntry>> Entries;

	TWeakObjectPtr<AShooterPlayerController> BoundPlayerController;
};
