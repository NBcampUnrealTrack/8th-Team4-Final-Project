#include "UI/InGame/HUD/DominationAugmentSelectHint.h"

#include "CommonTextBlock.h"
#include "Augment/AugmentInventoryComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "UI/Frontend/DominationUIInitComponent.h"
#include "GameMode/State/DominationGameState.h"
#include "Engine/World.h"

void UDominationAugmentSelectHint::NativeConstruct()
{
	Super::NativeConstruct();
	
	ResetTransientState();
	TryBindDependencies();
}

void UDominationAugmentSelectHint::NativeDestruct()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DependencyBindRetryTimerHandle);
		World->GetTimerManager().ClearTimer(PresentationRefreshTimerHandle);
	}
	
	if (UDominationUIInitComponent* UIInit = BoundUIInit.Get())
	{
		UIInit->OnAugmentDismissStateChanged.RemoveDynamic(this, &ThisClass::HandleDismissStateChanged);
		UIInit->OnAugmentPresentationChanged.RemoveDynamic(this, &ThisClass::HandlePresentationChanged);
	}
	BoundUIInit.Reset();
	
	if (UAugmentInventoryComponent* Inv = BoundInv.Get())
	{
		BoundInv->OnOffersChanged.RemoveDynamic(this, &ThisClass::RefreshPresentation);
	}
	BoundInv.Reset();
	
	ResetTransientState();
	Super::NativeDestruct();
}

void UDominationAugmentSelectHint::SchedulePresentationRefresh()
{
	UWorld* World = GetWorld();
	if (bPresentationRefreshPending || !IsValid(World))
	{
		return;
	}
	
	bPresentationRefreshPending = true;
	PresentationRefreshTimerHandle = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &ThisClass::RefreshPresentation));
}

void UDominationAugmentSelectHint::TryBindDependencies()
{
	UWorld* World = GetWorld();
	
	// Domination 모드인지 확인한다.
	if (IsValid(World))
	{
		AGameStateBase* GameState = World->GetGameState();
		if (IsValid(GameState) && !GameState->IsA<ADominationGameState>())
		{
			ShutDownForOtherMode();
			return;
		}
	}
	
	UDominationUIInitComponent* UIInit = GetUIInitComp();
	UAugmentInventoryComponent* Inv = GetInventory();
	
	if (!IsValid(UIInit) || !IsValid(Inv))
	{
		// 데이터가 오지않았음 처리
		SetVisibility(ESlateVisibility::Collapsed);
		
		if (IsValid(World) && !World->GetTimerManager().IsTimerActive(DependencyBindRetryTimerHandle))
		{
			World->GetTimerManager().SetTimer(
				DependencyBindRetryTimerHandle,
				this,
				&ThisClass::TryBindDependencies,
				DependencyBindRetryInterval,
				true);
		}
		return;
	}
	
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(DependencyBindRetryTimerHandle);
	}
	
	BoundUIInit = UIInit;
	UIInit->OnAugmentDismissStateChanged.AddUniqueDynamic(
		this, &ThisClass::HandleDismissStateChanged);
	UIInit->OnAugmentPresentationChanged.AddUniqueDynamic(
		this, &ThisClass::HandlePresentationChanged);
	
	BoundInv = Inv;
	Inv->OnOffersChanged.AddUniqueDynamic(
		this, &ThisClass::HandleOffersChanged);
	
	RefreshPresentation();
}

void UDominationAugmentSelectHint::ShutDownForOtherMode()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DependencyBindRetryTimerHandle);
		World->GetTimerManager().ClearTimer(PresentationRefreshTimerHandle);
	}
	
	SetVisibility(ESlateVisibility::Collapsed);
}

void UDominationAugmentSelectHint::ResetTransientState()
{
	// 아직 한 번도 표시/숨김 상태를 적용하지 않았는지 추적한다.
	bHasVisibilityState = false;
	// 마지막으로 계산된 표시 여부.
	// true면 직전 상태가 표시, false면 숨김이었다는 뜻.
	bLastShouldShow = false;
	// 다음 틱에 Presentation 갱신이 이미 예약되어 있는지 여부.
	// 같은 프레임에 여러 갱신 요청이 들어와도 중복 예약하지 않게 한다.
	bPresentationRefreshPending = false;
	
	DisplaySeconds = 0;
}

void UDominationAugmentSelectHint::RefreshPresentation()
{
	bPresentationRefreshPending = false;
	
	const UDominationUIInitComponent* UIInit = BoundUIInit.Get();
	const UAugmentInventoryComponent* Inv = BoundInv.Get();
	
	const bool bShouldShow = IsValid(UIInit)
		&& IsValid(Inv)
		&& !Inv->GetOffers().IsEmpty()
		&& UIInit->IsAugmentSelectDismissed()
		&& UIInit->IsAugmentPresentationSettled();
	
	const bool bVisibilityChanged = !bHasVisibilityState || bLastShouldShow != bShouldShow;
	bHasVisibilityState = true;
	bLastShouldShow = bShouldShow;
	
	if (bVisibilityChanged)
	{
		SetVisibility(bShouldShow
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
		
		BP_OnHintVisibilityChanged(bShouldShow);
	}
	
	// 보류 중에 다음 Draft가 이어 받기 위함 카운트다운은 계속 돌린다.
	if (bShouldShow)
	{
		StartCountdown();
	}
	else
	{
		StopCountdown();
	}
}

void UDominationAugmentSelectHint::RefreshCountdown()
{
	const UAugmentInventoryComponent* Inventory = BoundInv.Get();
	const AGameStateBase* GameState =
		GetWorld() ? GetWorld()->GetGameState() : nullptr;

	if (!IsValid(Inventory) || !IsValid(GameState))
	{
		StopCountdown();
		return;
	}

	const float Deadline = Inventory->GetSelectionDeadline();
	if (Deadline <= 0.f)
	{
		StopCountdown();
		return;
	}

	const float Remaining =
		Deadline - GameState->GetServerWorldTimeSeconds();

	if (Remaining <= 0.f)
	{
		Text_RemainingSeconds->SetText(FText::GetEmpty());
		StopCountdown();
		return;
	}

	const float Progress = 1.f - FMath::Clamp(Remaining / TotalDuration, 0.f, 1.f);
	BP_OnCountdownProgressChanged(Progress);
	
	const int32 NewSeconds = FMath::CeilToInt(Remaining);
	if (NewSeconds == DisplaySeconds)
	{
		return;
	}

	DisplaySeconds = NewSeconds;

	Text_RemainingSeconds->SetText(FText::AsNumber(DisplaySeconds));
}

void UDominationAugmentSelectHint::StartCountdown()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	RefreshCountdown();

	World->GetTimerManager().SetTimer(
		CountdownTimerHandle,
		this,
		&ThisClass::RefreshCountdown,
		0.1f,
		true);
}

void UDominationAugmentSelectHint::StopCountdown()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CountdownTimerHandle);
	}
}

void UDominationAugmentSelectHint::HandleOffersChanged()
{
	SchedulePresentationRefresh();
}

void UDominationAugmentSelectHint::HandleDismissStateChanged()
{
	// Setter를 거치므로 프레임당 한 번이고, B키 반응이 한틱 늦으면 체감되므로 즉시 처리
	RefreshPresentation();
}

void UDominationAugmentSelectHint::HandlePresentationChanged()
{
	RefreshPresentation();
}

UAugmentInventoryComponent* UDominationAugmentSelectHint::GetInventory() const
{
	const APlayerController* PC = GetOwningPlayer();
	AShooterPlayerState* PS = IsValid(PC)
		? PC->GetPlayerState<AShooterPlayerState>()
		: nullptr;
	
	return IsValid(PS) ? PS->GetAugmentInventory() : nullptr;
}

UDominationUIInitComponent* UDominationAugmentSelectHint::GetUIInitComp() const
{
	APlayerController* PC = GetOwningPlayer();
	return IsValid(PC)
		? PC->FindComponentByClass<UDominationUIInitComponent>()
		: nullptr;
}
