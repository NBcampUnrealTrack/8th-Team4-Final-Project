// ShooterRerollButton.h
#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "ShooterRerollButton.generated.h"

class UCommonTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRerollButtonClicked, int32 /*SlotIndex*/);

/**
 * 증강 카드 슬롯 하나마다 있는 리롤 버튼
 * 잔여 횟수 0이면 비활성화
 */
UCLASS(Abstract)
class FPS_API UShooterRerollButton : public UCommonButtonBase
{
	GENERATED_BODY()
	
public:
	// 슬롯 인덱스와 잔여 횟수를 함께 넣는다
	void SetSlotData(int32 InSlotIndex, int32 InRerollRemaining);
	
	FOnRerollButtonClicked OnRerollButtonClicked;
	
protected:
	virtual void NativeOnClicked() override;
	
	void RecoverFromRequestGuard();
	
	// 잔여 횟수 표시
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_RerollsRemaining;
	
private:
	int32 SlotIndex = INDEX_NONE;
	int32 RerollsRemaining = 0;
	
	// 서버 요청 전 입력 방지 Timer
	FTimerHandle RequestGuardTimerHandle;
};
