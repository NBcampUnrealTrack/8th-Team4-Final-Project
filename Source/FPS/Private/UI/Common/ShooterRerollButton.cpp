// ShooterRerollButton.cpp
#include "FPS/Public/UI/Common/ShooterRerollButton.h"

#include "CommonTextBlock.h"

void UShooterRerollButton::NativeOnClicked()
{
	Super::NativeOnClicked();
	
	// 인덱스가 안 들어온 버튼은 아무것도 하지않음
	if (SlotIndex == INDEX_NONE || RerollsRemaining <= 0)
	{
		return;
	}
	
	// 서버 응답 전 연타로 리롤 요청이 중복 전송되는 것을 막기 위해 false처리 
	// 정상 갱신이 오면 SetSlotData에서 즉시 해제하고 서버가 요청 거부한 경우 타임아웃으로 자체 복구
	SetIsEnabled(false);
	OnRerollButtonClicked.Broadcast(SlotIndex);
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RequestGuardTimerHandle,
			this,
			&ThisClass::RecoverFromRequestGuard,
			0.5f,
			false);
	}
}

void UShooterRerollButton::SetSlotData(int32 InSlotIndex, int32 InRerollRemaining)
{
	SlotIndex = InSlotIndex;
	RerollsRemaining = FMath::Max(0, InRerollRemaining);
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RequestGuardTimerHandle);
	}
	
	// 남은 횟수가 없으면 버튼 비활성화
	SetIsEnabled(RerollsRemaining > 0);
	
	if (Text_RerollsRemaining)
	{
		Text_RerollsRemaining->SetText(FText::AsNumber(RerollsRemaining));
	}
}

void UShooterRerollButton::RecoverFromRequestGuard()
{
	UE_LOG(LogTemp, Warning, TEXT("[ShooterRerollButton] Reroll request timeout"));
	SetIsEnabled(RerollsRemaining > 0);
}