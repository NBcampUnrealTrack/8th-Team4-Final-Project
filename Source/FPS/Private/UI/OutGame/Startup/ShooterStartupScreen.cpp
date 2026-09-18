// ShooterStartupScreen.cpp
#include "UI/OutGame/Startup/ShooterStartupScreen.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Tags/UITags.h"
#include "UI/Foundation/ShooterButtonBase.h"
#include "UI/OutGame/ShooterOutGameSubsystem.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"

namespace
{
	enum class EStartupOnlineState : uint8
	{
		Local,
		Connected,
		Offline,
		SteamUnavailable
	};

	bool IsLocalRunIntended(const UWorld* World)
	{
		if (World->IsPlayInEditor())
		{
			return true;
		}

#if (!UE_BUILD_SHIPPING && !UE_BUILD_SHIPPING_WITH_EDITOR) || ENABLE_PGO_PROFILE
		// 엔진이 -nosteam 을 받아들이는 빌드에서만 의도로 본다
		if (FParse::Param(FCommandLine::Get(), TEXT("nosteam")))
		{
			return true;
		}
#endif

		FString PlatformService;
		GConfig->GetString(TEXT("OnlineSubsystem"), TEXT("DefaultPlatformService"), PlatformService, GEngineIni);
		return PlatformService.Equals(TEXT("NULL"), ESearchCase::IgnoreCase);
	}

	EStartupOnlineState ClassifyOnlineState(const UWorld* World, const ULocalPlayer* LocalPlayer)
	{
		// 실제 OSS 가 STEAM 이면 실행 의도와 상관없이 Steam 분기
		const IOnlineSubsystem* Subsystem = Online::GetSubsystem(World);
		if (Subsystem && Subsystem->GetSubsystemName() == TEXT("STEAM"))
		{
			const IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
			const int32 LocalUserNum = IsValid(LocalPlayer) ? LocalPlayer->GetControllerId() : 0;
			return Identity.IsValid() && Identity->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn
				? EStartupOnlineState::Connected
				: EStartupOnlineState::Offline;
		}

		return IsLocalRunIntended(World)
			? EStartupOnlineState::Local
			: EStartupOnlineState::SteamUnavailable;
	}
}

void UShooterStartupScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Btn_Login->OnClicked().AddUObject(this, &ThisClass::HandleLoginClicked);
	Btn_Retry->OnClicked().AddUObject(this, &ThisClass::HandleRetryClicked);
	Btn_Quit->OnClicked().AddUObject(this, &ThisClass::HandleQuitClicked);

	IntroFinishedDelegate.BindDynamic(this, &ThisClass::HandleIntroFinished);
	BindToAnimationFinished(Anim_Intro, IntroFinishedDelegate);

	OpenFinishedDelegate.BindDynamic(this, &ThisClass::HandleOpenFinished);
	BindToAnimationFinished(Anim_Open, OpenFinishedDelegate);
}

void UShooterStartupScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	Text_Status->SetVisibility(ESlateVisibility::Collapsed);
	Btn_Login->SetVisibility(ESlateVisibility::Collapsed);
	Btn_Retry->SetVisibility(ESlateVisibility::Collapsed);
	Btn_Quit->SetVisibility(ESlateVisibility::Collapsed);

	// 로고 애니메이션과 BGM 요청은 같은 프레임
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UShooterOutGameSubsystem* OutGame = GameInstance->GetSubsystem<UShooterOutGameSubsystem>())
		{
			OutGame->SetMusicState(EShooterOutGameMusicState::Frontend);
		}
	}

	PlayAnimation(Anim_Intro);
}

void UShooterStartupScreen::HandleIntroFinished()
{
	RefreshOnlineState();
}

void UShooterStartupScreen::RefreshOnlineState()
{
	const EStartupOnlineState State = ClassifyOnlineState(GetWorld(), GetOwningLocalPlayer());

	FText StatusText;
	switch (State)
	{
	case EStartupOnlineState::Local:
		StatusText = NSLOCTEXT("ShooterLobby", "StartupLocalMode", "LOCAL MODE");
		break;
	case EStartupOnlineState::Connected:
		StatusText = NSLOCTEXT("ShooterLobby", "StartupConnected", "CONNECTED");
		break;
	case EStartupOnlineState::Offline:
		StatusText = NSLOCTEXT("ShooterLobby", "StartupSteamOffline", "Steam에 연결되어 있지 않습니다");
		break;
	case EStartupOnlineState::SteamUnavailable:
		StatusText = NSLOCTEXT("ShooterLobby", "StartupSteamUnavailable", "Steam을 실행한 뒤 게임을 재시작해 주세요");
		break;
	}

	const bool bCanLogin = State == EStartupOnlineState::Local
		|| State == EStartupOnlineState::Connected;

	Text_Status->SetText(StatusText);
	Text_Status->SetVisibility(ESlateVisibility::HitTestInvisible);
	Btn_Login->SetVisibility(bCanLogin ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Btn_Retry->SetVisibility(State == EStartupOnlineState::Offline
		? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Btn_Quit->SetVisibility(State == EStartupOnlineState::SteamUnavailable
		? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	// 활성화 때는 버튼이 숨어 있어 포커스를 못 줬다
	RequestRefreshFocus();
}

UWidget* UShooterStartupScreen::NativeGetDesiredFocusTarget() const
{
	// QUIT 은 기본 포커스로 두지 않는다
	if (Btn_Login->IsVisible())
	{
		return Btn_Login;
	}
	if (Btn_Retry->IsVisible())
	{
		return Btn_Retry;
	}
	return nullptr;
}

void UShooterStartupScreen::HandleLoginClicked()
{
	if (bOpening)
	{
		return;
	}

	const TSubclassOf<UCommonActivatableWidget> LoadedClass = HomeScreenClass.LoadSynchronous();
	if (!IsValid(LoadedClass))
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UShooterUIManagerSubsystem* UIManager = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr;
	if (!IsValid(UIManager))
	{
		return;
	}

	bOpening = true;

	UIManager->PushWidgetToLayer(UITags::UI_Layer_Menu, LoadedClass);

	Text_Status->SetVisibility(ESlateVisibility::Collapsed);
	Btn_Login->SetVisibility(ESlateVisibility::Collapsed);
	PlayAnimation(Anim_Open);
}

void UShooterStartupScreen::HandleOpenFinished()
{
	DeactivateWidget();
}

void UShooterStartupScreen::HandleRetryClicked()
{
	RefreshOnlineState();
}

void UShooterStartupScreen::HandleQuitClicked()
{
	// Steam 을 켠 뒤 다시 실행해야 하므로 확인창 없이 종료
	UKismetSystemLibrary::QuitGame(
		this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
