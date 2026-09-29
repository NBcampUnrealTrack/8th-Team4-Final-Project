// DominationAnnouncementBanner.cpp
#include "UI/InGame/HUD/DominationAnnouncementBanner.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameMode/State/DominationGameState.h"
#include "Player/BaseTeamPlayerState.h"
#include "UI/ShooterUISettings.h"

UDominationAnnouncementBanner::UDominationAnnouncementBanner(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MatchStartText = NSLOCTEXT("DominationBanner", "MatchStart", "MATCH START");
	ObjectivePrefixText = NSLOCTEXT("DominationBanner", "ObjectivePrefix", "OBJECTIVE");
	CaptureStartedAllyText = NSLOCTEXT("DominationBanner", "CaptureStartedAlly", "CAPTURING");
	CaptureStartedEnemyText = NSLOCTEXT("DominationBanner", "CaptureStartedEnemy", "CAPTURING");
	CaptureCompletedAllyText = NSLOCTEXT("DominationBanner", "CaptureCompletedAlly", "SECURED");
	CaptureCompletedEnemyText = NSLOCTEXT("DominationBanner", "CaptureCompletedEnemy", "LOST");
}

void UDominationAnnouncementBanner::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);

	ADominationGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ADominationGameState>() : nullptr;
	if (!GameState)
	{
		return;
	}

	BoundGameState = GameState;
	GameState->OnObjectiveFlowStateChanged.AddUniqueDynamic(this, &ThisClass::HandleObjectiveFlowStateChanged);
	GameState->OnCaptureStateChanged.AddUniqueDynamic(this, &ThisClass::HandleCaptureStateChanged);

	if (const TOptional<FDominationAnnouncement> Announcement = Detector.Initialize(BuildSnapshot(*GameState)))
	{
		PresentAnnouncement(*Announcement);
	}
}

void UDominationAnnouncementBanner::NativeDestruct()
{
	if (ADominationGameState* GameState = BoundGameState.Get())
	{
		GameState->OnObjectiveFlowStateChanged.RemoveDynamic(this, &ThisClass::HandleObjectiveFlowStateChanged);
		GameState->OnCaptureStateChanged.RemoveDynamic(this, &ThisClass::HandleCaptureStateChanged);
	}
	BoundGameState.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DisplayTimer);
	}

	Super::NativeDestruct();
}

void UDominationAnnouncementBanner::HandleObjectiveFlowStateChanged()
{
	EvaluateGameState();
}

void UDominationAnnouncementBanner::HandleCaptureStateChanged(FDominationCaptureState)
{
	// Capture와 Flow의 복제 순서가 엇갈릴 수 있어, 
	// 델리게이트 값 대신 GameState 전체를 Snapshot으로 다시 읽는다.
	EvaluateGameState();
}

void UDominationAnnouncementBanner::EvaluateGameState()
{
	const ADominationGameState* GameState = BoundGameState.Get();
	if (!GameState)
	{
		return;
	}

	if (const TOptional<FDominationAnnouncement> Announcement = Detector.Update(BuildSnapshot(*GameState)))
	{
		PresentAnnouncement(*Announcement);
	}
}

FDominationAnnouncementSnapshot UDominationAnnouncementBanner::BuildSnapshot(const ADominationGameState& GameState)
{
	FDominationAnnouncementSnapshot Snapshot;
	Snapshot.MatchPhase = GameState.GetDominationPhase();
	Snapshot.ObjectiveFlow = GameState.GetObjectiveFlowState();
	Snapshot.Capture = GameState.GetCaptureState();
	Snapshot.bCaptureEnabled = GameState.IsObjectiveCaptureEnabled();
	Snapshot.TransitionTimeRemaining = GameState.GetPointTransitionTimeRemaining();
	return Snapshot;
}

void UDominationAnnouncementBanner::PresentAnnouncement(const FDominationAnnouncement& Announcement)
{
	if (Announcement.Type == EDominationAnnouncementType::MatchStarted)
	{
		Text_Prefix->SetText(MatchStartText);
		Text_Point->SetVisibility(ESlateVisibility::Collapsed);
		Text_Result->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		const ABaseTeamPlayerState* LocalPlayerState = GetOwningPlayerState<ABaseTeamPlayerState>();
		const EGameTeam LocalTeam = LocalPlayerState ? LocalPlayerState->GetTeam() : EGameTeam::None;
		const EDominationCaptureRelation Relation =
			DominationRules::GetCaptureRelation(Announcement.EventTeam, LocalTeam);
		if (Relation == EDominationCaptureRelation::Neutral)
		{
			// 팀을 모르면 생략
			return;
		}

		const bool bAlly = Relation == EDominationCaptureRelation::Ally;
		const bool bCompleted = Announcement.Type == EDominationAnnouncementType::CaptureCompleted;
		const FLinearColor RelationColor = bAlly
			? UShooterUISettings::GetAllyColor()
			: UShooterUISettings::GetEnemyColor();

		Text_Prefix->SetText(ObjectivePrefixText);
		Text_Point->SetText(UEnum::GetDisplayValueAsText(Announcement.PointId));
		Text_Point->SetColorAndOpacity(FSlateColor(RelationColor));
		Text_Point->SetVisibility(ESlateVisibility::HitTestInvisible);
		Text_Result->SetText(bCompleted
			? (bAlly ? CaptureCompletedAllyText : CaptureCompletedEnemyText)
			: (bAlly ? CaptureStartedAllyText : CaptureStartedEnemyText));
		Text_Result->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);

	// 재생 중이어도 처음부터	
	PlayAnimation(Anim_Show, 0.0f);

	const float DisplaySeconds = FMath::Max(0.1f, Anim_Show->GetEndTime());
	GetWorld()->GetTimerManager().SetTimer(
		DisplayTimer, this, &ThisClass::HandleDisplayExpired, DisplaySeconds, false);
}

void UDominationAnnouncementBanner::HandleDisplayExpired()
{
	StopAnimation(Anim_Show);
	SetVisibility(ESlateVisibility::Collapsed);
}
