#include "GameMode/DominationGameMode.h"

#include "GameFramework/PlayerController.h"
#include "GameMode/DominationGameState.h"
#include "Player/DominationPlayerController.h"

ADominationGameMode::ADominationGameMode()
{
	GameStateClass = ADominationGameState::StaticClass();
	PlayerControllerClass = ADominationPlayerController::StaticClass();
}

void ADominationGameMode::NotifyPlayerReadyForGameFlow(APlayerController* PlayerController)
{
	ADominationPlayerController* DominationPlayerController =
		Cast<ADominationPlayerController>(PlayerController);

	if (!HasAuthority()
		|| !IsValid(DominationPlayerController)
		|| !DominationPlayerController->HasAuthority()
		|| DominationPlayerController->GetWorld() != GetWorld())
	{
		return;
	}

	Super::NotifyPlayerReadyForGameFlow(PlayerController);

	// 현재 기본 구조에는 별도의 매치 시작 게이트가 없으므로 준비 통지와 함께 로딩을 종료한다.
	DominationPlayerController->ClientHideLoadingScreen();
}
