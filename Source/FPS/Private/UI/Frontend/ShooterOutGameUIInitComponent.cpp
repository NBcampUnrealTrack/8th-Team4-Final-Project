// ShooterOutGameUIInitComponent.cpp
#include "UI/Frontend/ShooterOutGameUIInitComponent.h"

#include "CommonActivatableWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Loading/LoadingScreenSubsystem.h"
#include "Tags/UITags.h"
#include "UI/OutGame/MatchResult/ShooterMatchResultSubsystem.h"
#include "UI/OutGame/ShooterOutGameSubsystem.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"
#include "UI/Transition/ScreenTransitionSubsystem.h"

void UShooterOutGameUIInitComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	if (!ensureMsgf(
		RootLayoutClass && InitialWidgetClass,
		TEXT("OutGame UI init failed: RootLayoutClass or InitialWidgetClass is not set on %s."),
		*GetNameSafe(GetOwner())))
	{
		// UI 가 없어도 덮개에 갇히지는 않는다
		EndScreenTransitionUnlessResultPending();
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!IsValid(UIManager))
	{
		EndScreenTransitionUnlessResultPending();
		return;
	}

	UIManager->InitializeUI(PlayerController, RootLayoutClass);

	if (BackdropWidgetClass)
	{
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Game, BackdropWidgetClass);
	}

	// 아웃게임 화면이 월드를 덮은 뒤 로딩을 걷음
	if (ULoadingScreenSubsystem* LoadingScreen = GetLoadingScreenSubsystem())
	{
		LoadingScreen->HideLoadingScreen();
	}

	UShooterOutGameSubsystem* OutGame = GetOutGameSubsystem();

	// 인트로는 Startup 클래스가 지정된 PC 에서만
	if (StartupWidgetClass && IsValid(OutGame) && !OutGame->HasShownStartupIntro())
	{
		OutGame->MarkStartupIntroShown();
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Transition, StartupWidgetClass);
		EndScreenTransitionUnlessResultPending();
		return;
	}

	UIManager->PushWidgetToLayer(UITags::UI_Layer_Menu, InitialWidgetClass);

	// 첫 화면이 올라간 뒤에 걷는다
	EndScreenTransitionUnlessResultPending();

	if (IsValid(OutGame))
	{
		OutGame->SetMusicState(MusicState);
	}
}

UShooterUIManagerSubsystem* UShooterOutGameUIInitComponent::GetUIManager() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr;
}

UShooterOutGameSubsystem* UShooterOutGameUIInitComponent::GetOutGameSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterOutGameSubsystem>()
		: nullptr;
}

ULoadingScreenSubsystem* UShooterOutGameUIInitComponent::GetLoadingScreenSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance)
		? GameInstance->GetSubsystem<ULoadingScreenSubsystem>()
		: nullptr;
}

void UShooterOutGameUIInitComponent::EndScreenTransitionUnlessResultPending() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	if (!IsValid(GameInstance))
	{
		return;
	}

	const UShooterMatchResultSubsystem* MatchResult = GameInstance->GetSubsystem<UShooterMatchResultSubsystem>();
	if (IsValid(MatchResult) && MatchResult->GetSnapshot().bValid)
	{
		return;
	}

	if (UScreenTransitionSubsystem* Transition = GameInstance->GetSubsystem<UScreenTransitionSubsystem>())
	{
		Transition->EndTransition();
	}
}
