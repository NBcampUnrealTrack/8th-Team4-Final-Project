// ShooterMatchResultSubsystem.cpp
#include "UI/OutGame/MatchResult/ShooterMatchResultSubsystem.h"

#include "GameFramework/PlayerState.h"
#include "GameMode/State/BaseTeamMatchGameState.h"
#include "GameMode/State/LobbyGameState.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/ShooterPlayerState.h"
#include "UI/OutGame/MatchResult/ShooterMatchResultScreen.h"
#include "UI/ShooterUISettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterMatchResult, Log, All);

void UShooterMatchResultSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &ThisClass::HandlePostLoadMapWithWorld);
	WorldTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(
		this, &ThisClass::HandleWorldBeginTearDown);
}

void UShooterMatchResultSubsystem::Deinitialize()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}
	
	if (WorldTearDownHandle.IsValid())
	{
		FWorldDelegates::OnWorldBeginTearDown.Remove(WorldTearDownHandle);
		WorldTearDownHandle.Reset();
	}
	RemoveResultWidget();

	ClearEvaluateTimer();
	Snapshot.Reset();

	Super::Deinitialize();
}

void UShooterMatchResultSubsystem::CaptureSnapshot(
	const ABaseTeamMatchGameState* MatchGameState)
{
	// 중복 캡처 방지
	if (Snapshot.bValid)
	{
		return;
	}

	if (!IsValid(MatchGameState))
	{
		return;
	}

	// 무승부는 없으므로 처리하지않고 비정상으로 처리
	if (MatchGameState->GetMatchWinner() == EGameTeam::None)
	{
		UE_LOG(LogShooterMatchResult, Error, TEXT("Match가 끝났지만 승자가 없으므로 결과를 띄우지않습니다."));
		return;
	}

	APlayerController* LocalPC = GetGameInstance()->GetFirstLocalPlayerController();

	if (IsValid(LocalPC) == false)
	{
		return;
	}

	const ABaseTeamPlayerState* LocalPS = LocalPC->GetPlayerState<ABaseTeamPlayerState>();

	if (IsValid(LocalPS) == false)
	{
		return;
	}

	const EGameTeam LocalTeam = LocalPS->GetTeam();

	if (LocalTeam == EGameTeam::None)
	{
		return;
	}

	Snapshot.Reset();

	Snapshot.LocalTeam = LocalTeam;
	Snapshot.MatchWinner = MatchGameState->GetMatchWinner();
	Snapshot.TeamAScore = MatchGameState->GetTeamAScore();
	Snapshot.TeamBScore = MatchGameState->GetTeamBScore();

	for (APlayerState* PlayerState : MatchGameState->PlayerArray)
	{
		const ABaseTeamPlayerState* TeamPlayerState = Cast<ABaseTeamPlayerState>(PlayerState);

		if (IsValid(TeamPlayerState) == false)
		{
			continue;
		}

		const EGameTeam Team = TeamPlayerState->GetTeam();

		if (Team == EGameTeam::None)
		{
			continue;
		}

		FShooterMatchResultRow Row;
		Row.PlayerName = FText::FromString(TeamPlayerState->GetPlayerName());
		Row.Team = Team;
		
		if (const AShooterPlayerState* ShooterPS = Cast<AShooterPlayerState>(TeamPlayerState))
		{
			Row.Kills  = ShooterPS->GetKills();
			Row.Deaths = ShooterPS->GetDeaths();
		}

		Snapshot.Rows.Add(Row);
	}

	Snapshot.bStatsAvailable = true;
	Snapshot.bValid = true;
}

void UShooterMatchResultSubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	// 내 GameInstance의 유효한 World만 처리한다.
	// PostLoadMapWithWorld는 프로세스 전역이라 PIE 다른 인스턴스의 World도 들어온다
	if (!IsValid(LoadedWorld) || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	ClearEvaluateTimer();

	PendingWorld = LoadedWorld;
	EvaluateAttemptsLeft = MaxEvaluateAttempts;
	EvaluateWorld();
}

void UShooterMatchResultSubsystem::EvaluateWorld()
{
	UWorld* World = PendingWorld.Get();
	if (!IsValid(World))
	{
		ClearEvaluateTimer();
		return;
	}

	AGameStateBase* GameState = World->GetGameState();
	if (GameState == nullptr)
	{
		if (EvaluateAttemptsLeft > 0)
		{
			--EvaluateAttemptsLeft;

			World->GetTimerManager().SetTimer(
				EvaluateTimerHandle,
				this,
				&ThisClass::EvaluateWorld,
				EvaluateInterval,
				false);

			return;
		}

		Snapshot.Reset();
		ClearEvaluateTimer();
		return;
	}
	
	// 로비 입장 시
	ALobbyGameState* LobbyGS = Cast<ALobbyGameState>(GameState);
	if (LobbyGS)
	{
		if (!Snapshot.bValid)
		{
			ClearEvaluateTimer();
			return;
		}
		
		APlayerController* LocalPC = GetGameInstance()->GetFirstLocalPlayerController();

		if (!IsValid(LocalPC))
		{
			if (EvaluateAttemptsLeft > 0)
			{
				--EvaluateAttemptsLeft;

				World->GetTimerManager().SetTimer(
					EvaluateTimerHandle,
					this,
					&ThisClass::EvaluateWorld,
					EvaluateInterval,
					false);

				return;
			}

			Snapshot.Reset();
			ClearEvaluateTimer();
			return;
		}

		if (!PresentResultWidget(LocalPC))
		{
			Snapshot.Reset();
		}

		ClearEvaluateTimer();
		return;
	}

	// 새 매치 입장 시
	ABaseTeamMatchGameState* MatchGS = Cast<ABaseTeamMatchGameState>(GameState);
	if (MatchGS)
	{
		Snapshot.Reset();
		ClearEvaluateTimer();
		return;
	}

	// 로비도 매치도 아닌 월드. 스냅샷은 캡처 직후 한 번의 월드 전환에만 유효하다
	Snapshot.Reset();
	ClearEvaluateTimer();
}

void UShooterMatchResultSubsystem::ClearEvaluateTimer()
{
	if (UWorld* World = PendingWorld.Get())
	{
		World->GetTimerManager().ClearTimer(EvaluateTimerHandle);
	}
	
	EvaluateTimerHandle.Invalidate();
	PendingWorld.Reset();
	EvaluateAttemptsLeft = 0;
}

bool UShooterMatchResultSubsystem::PresentResultWidget(APlayerController* OwningPlayer)
{
	// 이미 결과 화면이 떠 있으면 성공으로 본다.
	if (IsValid(ActiveWidget))
	{
		return true;
	}

	const UShooterUISettings* Settings = GetDefault<UShooterUISettings>();

	// 결과 화면 클래스가 프로젝트 설정에 지정되지 않음.
	if (Settings->MatchResultScreenClass.IsNull())
	{
		UE_LOG(
			LogShooterMatchResult,
			Error,
			TEXT("결과 화면 클래스가 지정되지 않았습니다.")
		);
		return false;
	}

	// SoftClass 참조를 실제 UClass로 로드한다.
	UClass* LoadedClass = Settings->MatchResultScreenClass.LoadSynchronous();

	if (!LoadedClass)
	{
		UE_LOG(
			LogShooterMatchResult,
			Error,
			TEXT("결과 화면 클래스를 로드하지 못했습니다.")
		);
		return false;
	}

	ActiveWidget =
		CreateWidget<UShooterMatchResultScreen>(OwningPlayer, LoadedClass);

	if (!IsValid(ActiveWidget))
	{
		UE_LOG(
			LogShooterMatchResult,
			Error,
			TEXT("결과 화면 위젯 생성에 실패했습니다.")
		);
		return false;
	}

	ActiveWidget->OnCloseRequested.AddUObject(
		this,
		&ThisClass::HandleCloseRequested
	);

	ActiveWidget->SetResultData(Snapshot);
	ActiveWidget->AddToViewport(9000);
	WidgetWorld = OwningPlayer->GetWorld();

	return true;
}

void UShooterMatchResultSubsystem::RemoveResultWidget()
{
	if (ActiveWidget)
	{
		ActiveWidget->OnCloseRequested.RemoveAll(this);
		ActiveWidget->RemoveFromParent();
		ActiveWidget = nullptr;
	}
	
	WidgetWorld.Reset();
}

void UShooterMatchResultSubsystem::HandleCloseRequested()
{
	RemoveResultWidget();
	Snapshot.Reset();
}

void UShooterMatchResultSubsystem::HandleWorldBeginTearDown(UWorld* TearingDownWorld)
{
	if (WidgetWorld.IsValid() && WidgetWorld.Get() == TearingDownWorld)
	{
		RemoveResultWidget();
	}
}


const FShooterMatchResultSnapshot& UShooterMatchResultSubsystem::GetSnapshot() const
{
	return Snapshot;
}
