#include "UI/InGame/HUD/DominationSiteTrackerWidget.h"

#include "Actor/DominationCapturePoint.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#if WITH_EDITOR
#include "Engine/Blueprint.h"
#endif
#include "UI/InGame/HUD/DominationSiteTrackerSlotWidget.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameMode/State/DominationGameState.h"
#include "Player/BaseTeamPlayerState.h"
#include "UI/ShooterUISettings.h"

namespace
{
	void SetWidgetShown(UWidget* Widget, const bool bShown,
		const ESlateVisibility WhenHidden = ESlateVisibility::Collapsed)
	{
		if (IsValid(Widget))
		{
			Widget->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : WhenHidden);
		}
	}

	void TintImage(UImage* Image, const FLinearColor& Color)
	{
		if (IsValid(Image))
		{
			Image->SetColorAndOpacity(Color);
		}
	}
}

void UDominationSiteTrackerWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너 미리보기에도 동일한 아군/적군 이미지 틴트를 적용한다.
	const FLinearColor AllyColor = UShooterUISettings::GetAllyColor();
	const FLinearColor EnemyColor = UShooterUISettings::GetEnemyColor();
	TintImage(Image_AllySlot, AllyColor);
	TintImage(Image_EnemySlot, EnemyColor);
	TintImage(Image_AllyChevron, AllyColor);
	TintImage(Image_EnemyChevron, EnemyColor);
}

void UDominationSiteTrackerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ADominationGameState* DominationGameState = GetWorld()
		? GetWorld()->GetGameState<ADominationGameState>()
		: nullptr;
	if (!IsValid(DominationGameState))
	{
		if (GetWorld() && GetWorld()->GetGameState())
		{
			SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	BoundGameState = DominationGameState;
	// Reconstructing the same HUD must reapply visibility and count text once.
	CachedAllyCaptureCount = CachedEnemyCaptureCount = INDEX_NONE;
	// Do not replay historical completion flashes on late join / HUD reconstruction.
	LastPresentationCaptureSerial = DominationGameState->GetObjectiveCaptureSerial();
	DominationGameState->OnActivePointChanged.AddUniqueDynamic(this, &ThisClass::HandleActivePointChanged);
	DominationGameState->OnCaptureStateChanged.AddUniqueDynamic(this, &ThisClass::HandleCaptureStateChanged);
	DominationGameState->OnTeamScoreChanged.AddUniqueDynamic(this, &ThisClass::HandleTeamScoreChanged);
	DominationGameState->OnObjectiveFlowStateChanged.AddUniqueDynamic(this, &ThisClass::HandleObjectiveFlowStateChanged);

	// 위젯이 라운드 시작 이후에 만들어졌을 수 있으므로 현재 상태를 즉시 1회 반영
	HandleActivePointChanged(DominationGameState->GetActivePointId());
	HandleCaptureStateChanged(DominationGameState->GetCaptureState());
	BP_OnRoundScoreChanged(DominationGameState->GetTeamAScore(), DominationGameState->GetTeamBScore());
	RefreshRelativeScore(true);
	BP_OnTransitionStateChanged(IsPointTransitioning());

	GetWorld()->GetTimerManager().SetTimer(
		LocalCaptureZoneTimerHandle,
		this,
		&ThisClass::RefreshLocalState,
		LocalCaptureZoneUpdateInterval,
		true);
}

void UDominationSiteTrackerWidget::NativeDestruct()
{
	if (UHorizontalBox* Sites = Cast<UHorizontalBox>(GetWidgetFromName(TEXT("HBox_Sites"))))
	{
		for (int32 Index = 0; Index < Sites->GetChildrenCount(); ++Index)
		{
			if (UDominationSiteTrackerSlotWidget* Marker = Cast<UDominationSiteTrackerSlotWidget>(Sites->GetChildAt(Index)))
			{
				Marker->StopPresentationAnimations();
			}
		}
	}
	LastPresentationCaptureSerial = INDEX_NONE;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	if (ADominationGameState* DominationGameState = BoundGameState.Get())
	{
		DominationGameState->OnActivePointChanged.RemoveDynamic(this, &ThisClass::HandleActivePointChanged);
		DominationGameState->OnCaptureStateChanged.RemoveDynamic(this, &ThisClass::HandleCaptureStateChanged);
		DominationGameState->OnTeamScoreChanged.RemoveDynamic(this, &ThisClass::HandleTeamScoreChanged);
		DominationGameState->OnObjectiveFlowStateChanged.RemoveDynamic(this, &ThisClass::HandleObjectiveFlowStateChanged);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocalCaptureZoneTimerHandle);
	}

	if (ADominationCapturePoint* CapturePoint = ActiveCapturePoint.Get())
	{
		CapturePoint->OnPawnEntered.RemoveAll(this);
		CapturePoint->OnPawnExited.RemoveAll(this);
	}

	ActiveCapturePoint.Reset();
	BoundGameState.Reset();
	Super::NativeDestruct();
}

EDominationPointId UDominationSiteTrackerWidget::GetActivePointId() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) ? DominationGameState->GetActivePointId() : EDominationPointId::None;
}

int32 UDominationSiteTrackerWidget::GetActivePointIndex() const
{
	return DominationRules::GetPointIndex(GetActivePointId());
}

int32 UDominationSiteTrackerWidget::GetPointCount() const
{
	return DominationRules::PointCount;
}

TArray<EDominationPointId> UDominationSiteTrackerWidget::GetAllPointIds() const
{
	TArray<EDominationPointId> PointIds;
	PointIds.Reserve(DominationRules::PointCount);
	for (int32 Index = 0; Index < DominationRules::PointCount; ++Index)
	{
		PointIds.Add(DominationRules::GetPointId(Index));
	}

	return PointIds;
}

float UDominationSiteTrackerWidget::GetCaptureProgress() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) ? DominationGameState->GetCaptureProgress() : 0.0f;
}

EGameTeam UDominationSiteTrackerWidget::GetCapturingTeam() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) ? DominationGameState->GetCapturingTeam() : EGameTeam::None;
}

EDominationCaptureRelation UDominationSiteTrackerWidget::GetCapturingRelation() const
{
	const ABaseTeamPlayerState* LocalPlayerState = GetOwningPlayerState<ABaseTeamPlayerState>();
	const EGameTeam LocalTeam = IsValid(LocalPlayerState) ? LocalPlayerState->GetTeam() : EGameTeam::None;

	return DominationRules::GetCaptureRelation(GetCapturingTeam(), LocalTeam);
}

bool UDominationSiteTrackerWidget::IsCaptureContested() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) && DominationGameState->IsCaptureContested();
}

int32 UDominationSiteTrackerWidget::GetTeamAScore() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) ? DominationGameState->GetTeamAScore() : 0;
}

int32 UDominationSiteTrackerWidget::GetTeamBScore() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) ? DominationGameState->GetTeamBScore() : 0;
}

bool UDominationSiteTrackerWidget::IsLocalPlayerInsideActivePoint() const
{
	return bLocalPlayerInsideActivePoint;
}

bool UDominationSiteTrackerWidget::IsPointTransitioning() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState) && DominationGameState->IsPointTransitioning();
}

int32 UDominationSiteTrackerWidget::GetTransitionSecondsRemaining() const
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	return IsValid(DominationGameState)
		? FMath::Max(0, FMath::CeilToInt(DominationGameState->GetPointTransitionTimeRemaining()))
		: 0;
}

void UDominationSiteTrackerWidget::HandleActivePointChanged(const EDominationPointId ActivePointId)
{
	BP_OnActivePointChanged(ActivePointId, DominationRules::GetPointIndex(ActivePointId));

	RefreshActiveCapturePoint();
	RefreshLocalCaptureZoneState();
	RefreshSitePresentation();
}

void UDominationSiteTrackerWidget::HandleCaptureStateChanged(const FDominationCaptureState CaptureState)
{
	BP_OnCaptureStateChanged(GetCapturingRelation(), CaptureState.CaptureProgress, CaptureState.bContested);
	RefreshCapturePlayerCount();
	RefreshSitePresentation();
}

void UDominationSiteTrackerWidget::HandleTeamScoreChanged(const EGameTeam, const int32)
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	if (IsValid(DominationGameState))
	{
		BP_OnRoundScoreChanged(DominationGameState->GetTeamAScore(), DominationGameState->GetTeamBScore());
	}

	RefreshRelativeScore(true);
}

void UDominationSiteTrackerWidget::HandleObjectiveFlowStateChanged()
{
	BP_OnTransitionStateChanged(IsPointTransitioning());
	RefreshSitePresentation();
}

void UDominationSiteTrackerWidget::HandleCapturePointPawnChanged(APawn*)
{
	RefreshLocalCaptureZoneState();
}

void UDominationSiteTrackerWidget::RefreshActiveCapturePoint()
{
	if (ADominationCapturePoint* PreviousCapturePoint = ActiveCapturePoint.Get())
	{
		PreviousCapturePoint->OnPawnEntered.RemoveAll(this);
		PreviousCapturePoint->OnPawnExited.RemoveAll(this);
	}
	ActiveCapturePoint.Reset();

	const ADominationGameState* DominationGameState = BoundGameState.Get();
	UWorld* World = GetWorld();
	if (!IsValid(DominationGameState) || !IsValid(World))
	{
		return;
	}

	const EDominationPointId ActivePointId = DominationGameState->GetActivePointId();
	for (TActorIterator<ADominationCapturePoint> Iterator(World); Iterator; ++Iterator)
	{
		if (Iterator->GetPointId() == ActivePointId)
		{
			ActiveCapturePoint = *Iterator;
			Iterator->OnPawnEntered.AddUObject(this, &ThisClass::HandleCapturePointPawnChanged);
			Iterator->OnPawnExited.AddUObject(this, &ThisClass::HandleCapturePointPawnChanged);
			return;
		}
	}
}

void UDominationSiteTrackerWidget::RefreshLocalCaptureZoneState()
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	const ADominationCapturePoint* CapturePoint = ActiveCapturePoint.Get();
	const APawn* OwningPawn = GetOwningPlayerPawn();
	const ABaseTeamPlayerState* PlayerState = IsValid(OwningPawn)
		? OwningPawn->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;
	const bool bShouldShow = IsValid(DominationGameState)
		&& DominationGameState->IsObjectiveCaptureEnabled()
		&& IsValid(CapturePoint)
		&& IsValid(PlayerState)
		&& PlayerState->IsAlive()
		&& CapturePoint->ContainsWorldLocation(OwningPawn->GetActorLocation());

	if (bHasLocalCaptureZoneState && bLocalPlayerInsideActivePoint == bShouldShow)
	{
		return;
	}

	bHasLocalCaptureZoneState = true;
	bLocalPlayerInsideActivePoint = bShouldShow;
	BP_OnLocalCaptureZoneChanged(bShouldShow);
	RefreshSitePresentation();
}

void UDominationSiteTrackerWidget::RefreshLocalState()
{
	RefreshLocalCaptureZoneState();
	// 위젯이 만들어질 때 내 팀이 아직 복제 전일 수 있어 도착하면 다시 보낸다
	RefreshRelativeScore(false);
	RefreshCapturePlayerCount();
}

void UDominationSiteTrackerWidget::RefreshRelativeScore(const bool bForce)
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	if (!IsValid(DominationGameState))
	{
		return;
	}

	const ABaseTeamPlayerState* LocalPlayerState = GetOwningPlayerState<ABaseTeamPlayerState>();
	const EGameTeam LocalTeam = IsValid(LocalPlayerState) ? LocalPlayerState->GetTeam() : EGameTeam::None;
	if (!bForce && LocalTeam == CachedLocalTeam)
	{
		return;
	}
	CachedLocalTeam = LocalTeam;

	// 팀을 아직 모르면 TeamA 를 우리 팀 자리에 둔다
	const bool bLocalIsTeamB = LocalTeam == EGameTeam::TeamB;
	const int32 TeamAScore = DominationGameState->GetTeamAScore();
	const int32 TeamBScore = DominationGameState->GetTeamBScore();
	BP_OnRelativeScoreChanged(
		bLocalIsTeamB ? TeamBScore : TeamAScore,
		bLocalIsTeamB ? TeamAScore : TeamBScore);
	RefreshSitePresentation();
}

void UDominationSiteTrackerWidget::RefreshCapturePlayerCount()
{
	const ADominationGameState* DominationGameState = BoundGameState.Get();
	if (!IsValid(DominationGameState))
	{
		return;
	}

	// 팀을 아직 모르면 TeamA 를 우리 팀 자리에 둔다
	const ABaseTeamPlayerState* LocalPlayerState = GetOwningPlayerState<ABaseTeamPlayerState>();
	const bool bLocalIsTeamB = IsValid(LocalPlayerState) && LocalPlayerState->GetTeam() == EGameTeam::TeamB;
	const FDominationCaptureState CaptureState = DominationGameState->GetCaptureState();
	const int32 AllyCount = bLocalIsTeamB ? CaptureState.TeamBPlayerCount : CaptureState.TeamAPlayerCount;
	const int32 EnemyCount = bLocalIsTeamB ? CaptureState.TeamAPlayerCount : CaptureState.TeamBPlayerCount;
	// 진행률 때문에 0.1초마다 불리므로 인원이 같으면 건너뛴다
	if (AllyCount == CachedAllyCaptureCount && EnemyCount == CachedEnemyCaptureCount)
	{
		return;
	}
	CachedAllyCaptureCount = AllyCount;
	CachedEnemyCaptureCount = EnemyCount;

	if (IsValid(Text_AllyCount))
	{
		Text_AllyCount->SetText(FText::AsNumber(AllyCount));
	}
	if (IsValid(Text_EnemyCount))
	{
		Text_EnemyCount->SetText(FText::AsNumber(EnemyCount));
	}
	SetWidgetShown(Panel_CaptureCount, AllyCount + EnemyCount > 0);
	// Hidden retains each reserved side; the centre icon never follows the occupied side.
	SetWidgetShown(Panel_AllyCount, AllyCount > 0, ESlateVisibility::Hidden);
	SetWidgetShown(Panel_EnemyCount, EnemyCount > 0, ESlateVisibility::Hidden);
	SetWidgetShown(Image_AllyChevron, AllyCount > 0 && EnemyCount == 0);
	SetWidgetShown(Image_EnemyChevron, EnemyCount > 0 && AllyCount == 0);
}

FString UDominationSiteTrackerWidget::GetRefinedWidgetParentPath(UObject* BlueprintAsset)
{
#if WITH_EDITOR
	const UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintAsset);
	return Blueprint && Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString();
#else
	return FString();
#endif
}

FString UDominationSiteTrackerWidget::GetRefinedBlueprintStatus(UObject* BlueprintAsset)
{
#if WITH_EDITOR
	const UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintAsset);
	if (!Blueprint || !Blueprint->GeneratedClass) return TEXT("MissingGeneratedClass");
	switch (Blueprint->Status)
	{
	case BS_UpToDate: return TEXT("Ready");
	case BS_UpToDateWithWarnings: return TEXT("ReadyWithWarnings");
	case BS_Error: return TEXT("Error");
	case BS_Dirty: return TEXT("Dirty");
	default: return TEXT("NotCompiled");
	}
#else
	return TEXT("EditorOnly");
#endif
}

void UDominationSiteTrackerWidget::RefreshSitePresentation()
{
	const ADominationGameState* GameState = BoundGameState.Get();
	if (!bEnableRefinedSitePresentation || !IsValid(GameState)) return;
	UHorizontalBox* Sites = Cast<UHorizontalBox>(GetWidgetFromName(TEXT("HBox_Sites")));
	if (!IsValid(Sites)) return;

	const FDominationCaptureState Capture = GameState->GetCaptureState();
	const FDominationObjectiveFlowState Flow = GameState->GetObjectiveFlowState();
	const bool bExactlyOneTeamPresent = (Capture.TeamAPlayerCount > 0) != (Capture.TeamBPlayerCount > 0);
	// Leftover progress in an empty point is not an ongoing capture.
	// Reversing the opponent's progress with one team present is an ongoing capture.
	const bool bCaptureRunning = GameState->IsObjectiveCaptureEnabled()
		&& bExactlyOneTeamPresent && !Capture.bContested && Capture.CaptureProgress < 1.0f;
	const ABaseTeamPlayerState* LocalState = GetOwningPlayerState<ABaseTeamPlayerState>();
	const EGameTeam LocalTeam = IsValid(LocalState) ? LocalState->GetTeam() : EGameTeam::None;
	auto RelativeColor = [LocalTeam](const EGameTeam Team)
	{
		switch (DominationRules::GetCaptureRelation(Team, LocalTeam))
		{
		case EDominationCaptureRelation::Ally: return UShooterUISettings::GetAllyColor();
		case EDominationCaptureRelation::Enemy: return UShooterUISettings::GetEnemyColor();
		default: return FLinearColor(0.91f, 0.94f, 0.97f, 1.0f);
		}
	};
	const FLinearColor ProgressColor = RelativeColor(Capture.CapturingTeam);
	const bool bNewCompletion = LastPresentationCaptureSerial != INDEX_NONE
		&& Flow.CaptureSerial > 0 && Flow.CaptureSerial != LastPresentationCaptureSerial;
	int32 Presented = 0;
	const int32 Count = FMath::Min(Sites->GetChildrenCount(), DominationRules::PointCount);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		UDominationSiteTrackerSlotWidget* Marker = Cast<UDominationSiteTrackerSlotWidget>(Sites->GetChildAt(Index));
		if (!IsValid(Marker)) continue;
		FDominationSiteMarkerState State;
		State.PointId = DominationRules::GetPointId(Index);
		State.bActive = State.PointId == Flow.ActivePointId;
		State.bCapturing = State.bActive && bCaptureRunning;
		State.bContested = State.bActive && Capture.bContested;
		State.bLocalFocus = State.bActive && bLocalPlayerInsideActivePoint;
		State.Progress = State.bActive ? Capture.CaptureProgress : 0.0f;
		State.ProgressColor = ProgressColor;
		Marker->PresentSiteState(State);
		if (bNewCompletion && State.PointId == Flow.LastCapturedPointId)
		{
			Marker->PresentCaptureComplete(RelativeColor(Flow.LastCapturingTeam));
		}
		++Presented;
	}
	if (Presented > 0) LastPresentationCaptureSerial = Flow.CaptureSerial;
}
