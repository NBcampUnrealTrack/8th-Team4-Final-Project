// ShooterMatchResultScreen.cpp
#include "UI/OutGame/MatchResult/ShooterMatchResultScreen.h"

#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"
#include "UI/OutGame/MatchResult/ShooterMatchResultRowWidget.h"

void UShooterMatchResultScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Btn_Close)
	{
		Btn_Close->OnClicked.AddDynamic(this, &ThisClass::HandleCloseClicked);
	}
}


void UShooterMatchResultScreen::SetResultData(const FShooterMatchResultSnapshot& InSnapshot)
{
	// 유효한 스냅샷은 Capture 단게에서 보장되어야함
	ensureMsgf(InSnapshot.MatchWinner != EGameTeam::None, TEXT("유효한 결과 스냅샷에 승자가 없습니다."));

	// 좌우매핑을 A/B로 고정으로 두지않고 MyTeam이 왼쪽에 오도록 한다
	const bool bLocalIsTeamA = (InSnapshot.LocalTeam == EGameTeam::TeamA);
	const int32 MyScore = bLocalIsTeamA ? InSnapshot.TeamAScore : InSnapshot.TeamBScore;
	const int32 EnemyScore = bLocalIsTeamA ? InSnapshot.TeamBScore : InSnapshot.TeamAScore;

	// 승패로 갈리는 판단은 이곳이 전부
	const bool bVictory = (InSnapshot.MatchWinner == InSnapshot.LocalTeam);

	if (Switcher_OutcomeVisual && Panel_Victory && Panel_Defeat)
	{
		Switcher_OutcomeVisual->SetActiveWidget(bVictory ? Panel_Victory : Panel_Defeat);
	}

	// 스코어 표시
	if (Text_MyTeamScore)
	{
		Text_MyTeamScore->SetText(FText::AsNumber(MyScore));
	}

	if (Text_EnemyTeamScore)
	{
		Text_EnemyTeamScore->SetText(FText::AsNumber(EnemyScore));
	}

	// 승리한 팀만 강조
	if (Highlight_MyTeam)
	{
		Highlight_MyTeam->SetVisibility(
			bVictory
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed
		);
	}

	if (Highlight_EnemyTeam)
	{
		Highlight_EnemyTeam->SetVisibility(
			bVictory
				? ESlateVisibility::Collapsed
				: ESlateVisibility::HitTestInvisible
		);
	}

	// 기존 플레이어 행 제거 후 다시 생성
	if (Box_MyTeamRows)
	{
		Box_MyTeamRows->ClearChildren();
	}

	if (Box_EnemyTeamRows)
	{
		Box_EnemyTeamRows->ClearChildren();
	}

	if (!RowWidgetClass)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("RowWidgetClass가 지정되지 않아 플레이어 행을 생성할 수 없습니다.")
		);
	}
	else
	{
		for (const FShooterMatchResultRow& Row : InSnapshot.Rows)
		{
			UShooterMatchResultRowWidget* RowWidget =
				CreateWidget<UShooterMatchResultRowWidget>(this, RowWidgetClass);

			if (!RowWidget)
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("MatchResult RowWidget 생성에 실패했습니다.")
				);
				continue;
			}

			const bool bIsLocal = (Row.Team == InSnapshot.LocalTeam);

			RowWidget->SetRowData(
				Row,
				InSnapshot.bStatsAvailable,
				bIsLocal
					? EShooterResultRowSide::LocalTeam
					: EShooterResultRowSide::EnemyTeam);

			if (bIsLocal)
			{
				if (Box_MyTeamRows)
				{
					Box_MyTeamRows->AddChild(RowWidget);
				}
			}
			else
			{
				if (Box_EnemyTeamRows)
				{
					Box_EnemyTeamRows->AddChild(RowWidget);
				}
			}
		}
	}

	// K/D/A 헤더 표시 여부
	if (Panel_StatsHeader)
	{
		Panel_StatsHeader->SetVisibility(
			InSnapshot.bStatsAvailable
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Hidden
		);
	}
}

void UShooterMatchResultScreen::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
}
