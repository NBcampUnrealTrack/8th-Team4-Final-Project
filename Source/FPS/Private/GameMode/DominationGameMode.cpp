#include "GameMode/DominationGameMode.h"

#include "GameFramework/PlayerController.h"
#include "GameMode/DominationGameState.h"
#include "GameMode/DominationLoadoutManager.h"
#include "GameMode/DominationMatchStartManager.h"
#include "Player/DominationPlayerController.h"
#include "Player/DominationPlayerState.h"

ADominationGameMode::ADominationGameMode()
{
	GameStateClass = ADominationGameState::StaticClass();
	PlayerControllerClass = ADominationPlayerController::StaticClass();
	PlayerStateClass = ADominationPlayerState::StaticClass();
	bDelayedStart = true;

	MatchStartManager = CreateDefaultSubobject<UDominationMatchStartManager>(TEXT("MatchStartManager"));
	LoadoutManager = CreateDefaultSubobject<UDominationLoadoutManager>(TEXT("LoadoutManager"));
}

void ADominationGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(MatchStartManager))
	{
		MatchStartManager->Initialize(this, RequiredPlayersPerTeam, ClassSelectionDuration);
	}
}

void ADominationGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(MatchStartManager))
	{
		MatchStartManager->Shutdown();
	}

	Super::EndPlay(EndPlayReason);
}

void ADominationGameMode::NotifyPlayerReadyForGameFlow(APlayerController* PlayerController)
{
	ADominationPlayerController* DominationPlayerController =
		Cast<ADominationPlayerController>(PlayerController);

	if (!IsValid(DominationPlayerController) || !IsValid(MatchStartManager))
	{
		return;
	}

	Super::NotifyPlayerReadyForGameFlow(PlayerController);
	MatchStartManager->NotifyPlayerLoaded();
}

void ADominationGameMode::RequestCombatClass(
	ADominationPlayerController* PlayerController,
	const EDominationCombatClass RequestedClass)
{
	if (IsValid(MatchStartManager))
	{
		MatchStartManager->RequestCombatClass(PlayerController, RequestedClass);
	}
}

UDominationMatchStartManager* ADominationGameMode::GetMatchStartManager() const
{
	return MatchStartManager.Get();
}

UDominationLoadoutManager* ADominationGameMode::GetLoadoutManager() const
{
	return LoadoutManager.Get();
}

void ADominationGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// BaseTeamGameMode의 Super 경로는 즉시 Pawn을 생성하므로 팀 배정만 수행한다.
	if (!InitializeTeamPlayer(NewPlayer))
	{
		return;
	}
}

bool ADominationGameMode::ShouldSpawnAtStartSpot(AController*)
{
	// 전진 스폰까지 동일한 선택 경로를 사용하므로 매 스폰마다 현재 팀 Start를 다시 찾는다.
	return false;
}
