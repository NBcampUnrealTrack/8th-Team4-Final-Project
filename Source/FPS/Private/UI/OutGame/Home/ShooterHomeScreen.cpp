// ShooterHomeScreen.cpp
#include "UI/OutGame/Home/ShooterHomeScreen.h"

#include "Engine/GameInstance.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Session/SessionSubSystem.h"
#include "Tags/UITags.h"
#include "UI/Common/ShooterConfirmDialog.h"
#include "UI/Foundation/ShooterButtonBase.h"
#include "UI/Settings/ShooterOptionsScreen.h"
#include "UI/ShooterUISettings.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"
#include "UI/Transition/ScreenTransitionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterHomeScreen, Log, All);

void UShooterHomeScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Btn_Play->OnClicked().AddUObject(this, &ThisClass::HandlePlayClicked);
	Btn_Training->OnClicked().AddUObject(this, &ThisClass::HandleTrainingClicked);
	Btn_Settings->OnClicked().AddUObject(this, &ThisClass::HandleSettingsClicked);
	Btn_Quit->OnClicked().AddUObject(this, &ThisClass::HandleQuitClicked);
	if (IsValid(Btn_Customize))
	{
		Btn_Customize->OnClicked().AddUObject(this, &ThisClass::HandleCustomizeClicked);
	}
}

void UShooterHomeScreen::NativeOnDeactivated()
{
	if (UShooterOptionsScreen* Screen = OptionsScreenInstance.Get())
	{
		Screen->OnCloseRequested.RemoveAll(this);
	}
	OptionsScreenInstance.Reset();
	ClearQuitConfirmBindings();

	Super::NativeOnDeactivated();
}

UWidget* UShooterHomeScreen::NativeGetDesiredFocusTarget() const
{
	return Btn_Play;
}

void UShooterHomeScreen::HandlePlayClicked()
{
	PushMenuScreen(SessionBrowserClass);
}

void UShooterHomeScreen::HandleTrainingClicked()
{
	if (TrainingDestinationMap.IsNull())
	{
		UE_LOG(LogShooterHomeScreen, Error,
			TEXT("TrainingDestinationMap is not configured."));
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	USessionSubSystem* Sessions = GameInstance ? GameInstance->GetSubsystem<USessionSubSystem>() : nullptr;
	if (!Sessions || !Sessions->LeaveRoomAndOpenLocalMap(TrainingDestinationMap))
	{
		UE_LOG(LogShooterHomeScreen, Warning,
			TEXT("Training entry requires one local player and no pending room transition."));
	}
}

void UShooterHomeScreen::HandleCustomizeClicked()
{
	if (CustomizationDestinationMap.IsNull())
	{
		UE_LOG(LogShooterHomeScreen, Error,
			TEXT("CustomizationDestinationMap is not configured."));
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	USessionSubSystem* Sessions = GameInstance ? GameInstance->GetSubsystem<USessionSubSystem>() : nullptr;
	UScreenTransitionSubsystem* Transition = GameInstance ? GameInstance->GetSubsystem<UScreenTransitionSubsystem>() : nullptr;
	if (!Sessions || (Transition && Transition->IsTransitionActive()))
	{
		return;
	}

	// 다 덮은 뒤 이동
	const FSimpleDelegate Request = FSimpleDelegate::CreateWeakLambda(
		Sessions,
		[Sessions, Transition, Destination = CustomizationDestinationMap]()
		{
			if (Sessions->LeaveRoomAndOpenLocalMap(Destination))
			{
				return;
			}

			UE_LOG(LogShooterHomeScreen, Warning,
				TEXT("Customization entry requires one local player and no pending room transition."));
			if (Transition)
			{
				Transition->EndTransition();
			}
		});

	if (!Transition || !Transition->BeginTransition(Request))
	{
		Request.ExecuteIfBound();
	}
}

void UShooterHomeScreen::HandleSettingsClicked()
{
	if (OptionsScreenInstance.IsValid())
	{
		return;
	}

	TSubclassOf<UShooterOptionsScreen> LoadedClass = OptionsScreenClass.LoadSynchronous();
	if (!IsValid(LoadedClass))
	{
		LoadedClass = UShooterUISettings::LoadDefaultOptionsScreenClass();
	}
	const UGameInstance* GameInstance = GetGameInstance();
	UShooterUIManagerSubsystem* UIManager = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr;
	if (!IsValid(UIManager) || !IsValid(LoadedClass))
	{
		UE_LOG(LogShooterHomeScreen, Error, TEXT("Options screen class is unavailable. Assign OptionsScreenClass or configure the default options screen."));
		return;
	}

	UShooterOptionsScreen* Screen = Cast<UShooterOptionsScreen>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, LoadedClass));
	if (!IsValid(Screen))
	{
		return;
	}

	Screen->OnCloseRequested.AddUObject(this, &ThisClass::HandleOptionsCloseRequested);
	OptionsScreenInstance = Screen;
}

void UShooterHomeScreen::HandleQuitClicked()
{
	if (QuitConfirmDialog.IsValid())
	{
		return;
	}

	const TSubclassOf<UShooterConfirmDialog> LoadedClass = ConfirmDialogClass.LoadSynchronous();
	const UGameInstance* GameInstance = GetGameInstance();
	UShooterUIManagerSubsystem* UIManager = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr;
	if (!IsValid(UIManager) || !IsValid(LoadedClass))
	{
		UE_LOG(LogShooterHomeScreen, Error, TEXT("ConfirmDialogClass is not configured."));
		return;
	}

	UShooterConfirmDialog* Dialog = Cast<UShooterConfirmDialog>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, LoadedClass));
	if (!IsValid(Dialog))
	{
		return;
	}

	Dialog->SetupMessage(NSLOCTEXT("ShooterLobby", "QuitGameConfirm", "게임을 종료하시겠습니까?"));
	Dialog->OnConfirmed.AddDynamic(this, &ThisClass::HandleQuitConfirmed);
	Dialog->OnCanceled.AddDynamic(this, &ThisClass::HandleQuitCanceled);
	QuitConfirmDialog = Dialog;
}

void UShooterHomeScreen::HandleQuitConfirmed()
{
	ClearQuitConfirmBindings();
	UKismetSystemLibrary::QuitGame(
		this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UShooterHomeScreen::HandleQuitCanceled()
{
	ClearQuitConfirmBindings();
}

void UShooterHomeScreen::ClearQuitConfirmBindings()
{
	if (UShooterConfirmDialog* Dialog = QuitConfirmDialog.Get())
	{
		Dialog->OnConfirmed.RemoveAll(this);
		Dialog->OnCanceled.RemoveAll(this);
	}
	QuitConfirmDialog.Reset();
}

void UShooterHomeScreen::HandleOptionsCloseRequested()
{
	UShooterOptionsScreen* Screen = OptionsScreenInstance.Get();
	if (!IsValid(Screen))
	{
		return;
	}

	Screen->FinishEditing();
	Screen->OnCloseRequested.RemoveAll(this);

	const UGameInstance* GameInstance = GetGameInstance();
	if (UShooterUIManagerSubsystem* UIManager = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr)
	{
		UIManager->RemoveWidgetFromLayer(UITags::UI_Layer_Modal, Screen);
	}

	OptionsScreenInstance.Reset();
}

void UShooterHomeScreen::PushMenuScreen(const TSoftClassPtr<UCommonActivatableWidget>& ScreenClass)
{
	const TSubclassOf<UCommonActivatableWidget> LoadedClass = ScreenClass.LoadSynchronous();
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

	UIManager->PushWidgetToLayer(UITags::UI_Layer_Menu, LoadedClass);
}
