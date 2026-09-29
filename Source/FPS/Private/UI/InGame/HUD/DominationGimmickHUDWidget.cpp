#include "UI/InGame/HUD/DominationGimmickHUDWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Player/BaseTeamPlayerState.h"
#include "Tags/GameplayTags.h"

ADominationPointGimmick* UDominationGimmickHUDWidget::GetGimmick() const
{
	return ActiveGimmick.Get();
}

EDominationGimmickType UDominationGimmickHUDWidget::GetGimmickType() const
{
	const ADominationPointGimmick* Gimmick = ActiveGimmick.Get();
	return IsValid(Gimmick) ? Gimmick->GetGimmickType() : EDominationGimmickType::Recon;
}


float UDominationGimmickHUDWidget::GetInteractionProgress() const
{
	const ADominationPointGimmick* Gimmick = ActiveGimmick.Get();
	return IsValid(Gimmick) ? Gimmick->GetInteractionProgress() : 0.0f;
}

EDominationCaptureRelation UDominationGimmickHUDWidget::GetInteractingRelation() const
{
	const ADominationPointGimmick* Gimmick = ActiveGimmick.Get();
	return IsValid(Gimmick)
		? DominationRules::GetCaptureRelation(Gimmick->GetInteractingTeam(), GetLocalTeam())
		: EDominationCaptureRelation::Neutral;
}

EDominationCaptureRelation UDominationGimmickHUDWidget::GetOwningRelation() const
{
	const ADominationPointGimmick* Gimmick = ActiveGimmick.Get();
	return IsValid(Gimmick)
		? DominationRules::GetCaptureRelation(Gimmick->GetOwningTeam(), GetLocalTeam())
		: EDominationCaptureRelation::Neutral;
}

int32 UDominationGimmickHUDWidget::GetCooldownSecondsRemaining() const
{
	const ADominationPointGimmick* Gimmick = ActiveGimmick.Get();
	return IsValid(Gimmick) ? FMath::CeilToInt(Gimmick->GetCooldownRemaining()) : 0;
}

EDominationCaptureRelation UDominationGimmickHUDWidget::GetMarkerRelation() const
{
	const EDominationCaptureRelation InteractingRelation = GetInteractingRelation();
	if (InteractingRelation != EDominationCaptureRelation::Neutral)
	{
		return InteractingRelation;
	}

	return GetGimmickType() == EDominationGimmickType::RespawnRelay
		? GetOwningRelation()
		: EDominationCaptureRelation::Neutral;
}

FText UDominationGimmickHUDWidget::GetMarkerLabel() const
{
	const int32 CooldownSeconds = GetCooldownSecondsRemaining();
	if (CooldownSeconds > 0)
	{
		return FText::Format(NSLOCTEXT("DominationGimmickHUD", "Cooldown", "대기 {0}초"), CooldownSeconds);
	}

	return FText::Format(
		NSLOCTEXT("DominationGimmickHUD", "Distance", "{0}m"),
		FMath::Max(0, FMath::RoundToInt(DistanceMeters)));
}

bool UDominationGimmickHUDWidget::ShouldShowPrompt() const
{
	const ADominationPointGimmick* Gimmick = ActiveGimmick.Get();
	return IsValid(Gimmick) && Gimmick->CanBeUsedBy(GetOwningPlayerPawn());
}

bool UDominationGimmickHUDWidget::IsLocalPlayerInteracting() const
{
	const ADominationPointGimmick* Gimmick = ActiveGimmick.Get();
	const APlayerState* LocalPlayerState = GetOwningPlayerState();
	return IsValid(Gimmick)
		&& IsValid(LocalPlayerState)
		&& Gimmick->GetInteractingPlayerState() == LocalPlayerState;
}

bool UDominationGimmickHUDWidget::IsLocalPlayerRevealed() const
{
	const UAbilitySystemComponent* AbilitySystem = GetLocalAbilitySystem();
	return IsValid(AbilitySystem)
		&& IsValid(GetOwningPlayerPawn())
		&& AbilitySystem->HasMatchingGameplayTag(RogueTags::State_Combat_Revealed);
}

void UDominationGimmickHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 기믹은 맵에 미리 배치되고 항상 로드되므로 생성 시 한 번만 구독한다.
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADominationPointGimmick> Iterator(World); Iterator; ++Iterator)
		{
			Iterator->OnGimmickStateChanged.AddUniqueDynamic(this, &ThisClass::HandleGimmickStateChanged);
			BoundGimmicks.Add(*Iterator);
		}
	}

	RefreshRevealedTagBinding();
	RefreshActiveGimmick();
}

void UDominationGimmickHUDWidget::NativeDestruct()
{
	UnbindRevealedTag();

	for (const TWeakObjectPtr<ADominationPointGimmick>& Gimmick : BoundGimmicks)
	{
		if (Gimmick.IsValid())
		{
			Gimmick->OnGimmickStateChanged.RemoveDynamic(this, &ThisClass::HandleGimmickStateChanged);
		}
	}

	BoundGimmicks.Reset();
	ActiveGimmick.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	Super::NativeDestruct();
}

void UDominationGimmickHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshRevealedTagBinding();

	// 이번 프레임에 온 상태 변경 이벤트를 한 번에 처리한다(마커 위치 갱신 포함).
	if (bRefreshPending)
	{
		bRefreshPending = false;
		RefreshActiveGimmick();
		return;
	}

	// 마커 위치만 카메라에 따라 매 프레임 갱신한다. 켜진 기믹이 없으면 아무것도 하지 않는다.
	if (ActiveGimmick.IsValid() || bShouldShowMarker)
	{
		RefreshMarkerPlacement();
		ApplyMarkerToWidget();
	}
}

void UDominationGimmickHUDWidget::HandleGimmickStateChanged(ADominationPointGimmick*)
{
	bRefreshPending = true;
}

void UDominationGimmickHUDWidget::HandleRevealedTagChanged(const FGameplayTag, int32)
{
	bRefreshPending = true;
}

UAbilitySystemComponent* UDominationGimmickHUDWidget::GetLocalAbilitySystem() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwningPlayerState());
}

void UDominationGimmickHUDWidget::RefreshRevealedTagBinding()
{
	UAbilitySystemComponent* AbilitySystem = GetLocalAbilitySystem();
	if (BoundAbilitySystem.Get() == AbilitySystem)
	{
		return;
	}

	UnbindRevealedTag();
	if (IsValid(AbilitySystem))
	{
		BoundAbilitySystem = AbilitySystem;
		RevealedTagHandle = AbilitySystem->RegisterGameplayTagEvent(
			RogueTags::State_Combat_Revealed,
			EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleRevealedTagChanged);
	}

	bRefreshPending = true;
}

void UDominationGimmickHUDWidget::UnbindRevealedTag()
{
	if (UAbilitySystemComponent* AbilitySystem = BoundAbilitySystem.Get())
	{
		AbilitySystem->RegisterGameplayTagEvent(
			RogueTags::State_Combat_Revealed,
			EGameplayTagEventType::NewOrRemoved).Remove(RevealedTagHandle);
	}

	BoundAbilitySystem.Reset();
	RevealedTagHandle.Reset();
}

void UDominationGimmickHUDWidget::HandleRefreshTimer()
{
	BP_OnGimmickHUDRefresh();
}

void UDominationGimmickHUDWidget::RefreshActiveGimmick()
{
	const ADominationPointGimmick* PreviousGimmick = ActiveGimmick.Get();
	ActiveGimmick = ADominationPointGimmick::GetActiveGimmick(this);

	// 아이콘처럼 기믹마다 한 번만 바꾸면 되는 표시는 켜진 기믹이 바뀔 때만 알린다.
	if (ActiveGimmick.Get() != PreviousGimmick)
	{
		BP_OnActiveGimmickChanged(ActiveGimmick.Get());
	}

	// 남은 초·거리·[F] 안내는 시간과 위치에 따라 바뀌므로 기믹이 켜진 동안만 주기적으로 갱신한다.
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (ActiveGimmick.IsValid() && !TimerManager.IsTimerActive(RefreshTimerHandle))
		{
			TimerManager.SetTimer(RefreshTimerHandle, this, &ThisClass::HandleRefreshTimer, RefreshInterval, true);
		}
		else if (!ActiveGimmick.IsValid())
		{
			TimerManager.ClearTimer(RefreshTimerHandle);
		}
	}

	RefreshMarkerPlacement();
	ApplyMarkerToWidget();
	ApplyAlertToWidget();
	BP_OnGimmickHUDRefresh();
}

void UDominationGimmickHUDWidget::RefreshMarkerPlacement()
{
	bShouldShowMarker = false;
	DistanceMeters = -1.0f;

	const ADominationPointGimmick* Gimmick = ActiveGimmick.Get();
	APlayerController* LocalController = GetOwningPlayer();
	if (!IsValid(Gimmick) || !IsValid(LocalController))
	{
		return;
	}

	// 캐릭터가 아니라 카메라 기준이라 사망 후 관전 중에도 마커가 보인다.
	FVector CameraLocation;
	FRotator CameraRotation;
	LocalController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector MarkerWorldLocation = Gimmick->GetActorLocation() + FVector(0.0f, 0.0f, MarkerHeightOffset);
	DistanceMeters = FVector::Dist(MarkerWorldLocation, CameraLocation) / 100.0f;

	FVector2D ProjectedPosition;
	if (!LocalController->ProjectWorldLocationToScreen(MarkerWorldLocation, ProjectedPosition, true))
	{
		return;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	LocalController->GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ProjectedPosition.X < 0.0f || ProjectedPosition.Y < 0.0f
		|| ProjectedPosition.X > ViewportSizeX || ProjectedPosition.Y > ViewportSizeY)
	{
		return;
	}

	// 해상도에 따른 위치 고정 (DominationObjectiveCompassWidget과 동일)
	const float ViewportScale = FMath::Max(KINDA_SMALL_NUMBER, UWidgetLayoutLibrary::GetViewportScale(this));
	MarkerScreenPosition = ProjectedPosition / ViewportScale;
	bShouldShowMarker = true;
}

void UDominationGimmickHUDWidget::ApplyMarkerToWidget()
{
	if (!IsValid(MarkerRoot))
	{
		return;
	}

	const ESlateVisibility NewVisibility = bShouldShowMarker
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;
	if (MarkerRoot->GetVisibility() != NewVisibility)
	{
		MarkerRoot->SetVisibility(NewVisibility);
	}

	if (bShouldShowMarker)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerRoot->Slot))
		{
			CanvasSlot->SetPosition(MarkerScreenPosition);
		}
	}
}

void UDominationGimmickHUDWidget::ApplyAlertToWidget()
{
	if (!IsValid(AlertRoot))
	{
		return;
	}

	const ESlateVisibility NewVisibility = IsLocalPlayerRevealed()
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed;
	if (AlertRoot->GetVisibility() != NewVisibility)
	{
		AlertRoot->SetVisibility(NewVisibility);
	}
}

EGameTeam UDominationGimmickHUDWidget::GetLocalTeam() const
{
	const ABaseTeamPlayerState* LocalPlayerState = GetOwningPlayerState<ABaseTeamPlayerState>();
	return IsValid(LocalPlayerState) ? LocalPlayerState->GetTeam() : EGameTeam::None;
}
