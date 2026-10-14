// DominationAnnouncementBanner.cpp
#include "UI/InGame/HUD/DominationAnnouncementBanner.h"

#include "Actor/DominationPointGimmick.h"
#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameMode/State/DominationGameState.h"
#include "Player/BaseTeamPlayerState.h"
#include "UI/ShooterUISettings.h"

UDominationAnnouncementBanner::UDominationAnnouncementBanner(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MatchStartText = NSLOCTEXT("DominationBanner", "MatchStart", "MATCH START");
	ObjectivePrefixText = NSLOCTEXT("DominationBanner", "ObjectivePrefix", "SITE");
	CaptureStartedAllyText = NSLOCTEXT("DominationBanner", "CaptureStartedAlly", "CAPTURING");
	CaptureStartedEnemyText = NSLOCTEXT("DominationBanner", "CaptureStartedEnemy", "CAPTURING");
	CaptureCompletedAllyText = NSLOCTEXT("DominationBanner", "CaptureCompletedAlly", "SECURED");
	CaptureCompletedEnemyText = NSLOCTEXT("DominationBanner", "CaptureCompletedEnemy", "LOST");
	ReconPrefixText = NSLOCTEXT("DominationBanner", "ReconPrefix", "RECON");
	RelayPrefixText = NSLOCTEXT("DominationBanner", "RelayPrefix", "RELAY");
	GimmickSecuredAllyText = NSLOCTEXT("DominationBanner", "GimmickSecuredAlly", "ONLINE");
	GimmickSecuredEnemyText = NSLOCTEXT("DominationBanner", "GimmickSecuredEnemy", "ONLINE");
	RelayTakeoverAllyText = NSLOCTEXT("DominationBanner", "RelayTakeoverAlly", "HIJACKED");
	RelayTakeoverEnemyText = NSLOCTEXT("DominationBanner", "RelayTakeoverEnemy", "LOST");
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

	// 기믹은 배치형/항상 로드/항상 Relevant이기 때문에 생성 시 한 번만 구독
	for (TActorIterator<ADominationPointGimmick> It(GetWorld()); It; ++It)
	{
		It->OnGimmickSecured.AddUniqueDynamic(this, &ThisClass::HandleGimmickSecured);
		BoundGimmicks.Add(*It);
	}

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

	for (const TWeakObjectPtr<ADominationPointGimmick>& Gimmick : BoundGimmicks)
	{
		if (Gimmick.IsValid())
		{
			Gimmick->OnGimmickSecured.RemoveDynamic(this, &ThisClass::HandleGimmickSecured);
		}
	}
	BoundGimmicks.Reset();

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
		const EDominationCaptureRelation Relation = GetLocalRelation(Announcement.EventTeam);
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

	ShowBanner();
}

void UDominationAnnouncementBanner::HandleGimmickSecured(
	ADominationPointGimmick* Gimmick,
	const EGameTeam SecuringTeam,
	const bool bTakeover)
{
	const ADominationGameState* GameState = BoundGameState.Get();
	// 기믹은 CaptureEnabled 동안만 켜진다. 아니면 이미 지난 알림
	if (!IsValid(Gimmick) || !GameState || !GameState->IsObjectiveCaptureEnabled())
	{
		return;
	}

	const EDominationCaptureRelation Relation = GetLocalRelation(SecuringTeam);
	if (Relation == EDominationCaptureRelation::Neutral)
	{
		return;
	}

	const bool bAlly = Relation == EDominationCaptureRelation::Ally;
	const bool bRelay = Gimmick->GetGimmickType() == EDominationGimmickType::RespawnRelay;
	const FLinearColor RelationColor = bAlly
		? UShooterUISettings::GetAllyColor()
		: UShooterUISettings::GetEnemyColor();

	Text_Prefix->SetText(bRelay ? RelayPrefixText : ReconPrefixText);
	Text_Point->SetText(UEnum::GetDisplayValueAsText(GameState->GetObjectiveFlowState().ActivePointId));
	Text_Point->SetColorAndOpacity(FSlateColor(RelationColor));
	Text_Point->SetVisibility(ESlateVisibility::HitTestInvisible);
	Text_Result->SetText(bTakeover
		? (bAlly ? RelayTakeoverAllyText : RelayTakeoverEnemyText)
		: (bAlly ? GimmickSecuredAllyText : GimmickSecuredEnemyText));
	Text_Result->SetVisibility(ESlateVisibility::HitTestInvisible);

	ShowBanner();
}

void UDominationAnnouncementBanner::ShowBanner()
{
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

EDominationCaptureRelation UDominationAnnouncementBanner::GetLocalRelation(const EGameTeam EventTeam) const
{
	const ABaseTeamPlayerState* LocalPlayerState = GetOwningPlayerState<ABaseTeamPlayerState>();
	const EGameTeam LocalTeam = LocalPlayerState ? LocalPlayerState->GetTeam() : EGameTeam::None;
	return DominationRules::GetCaptureRelation(EventTeam, LocalTeam);
}
