// ShooterCreateRoomDialog.cpp
#include "UI/OutGame/Session/ShooterCreateRoomDialog.h"

#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Session/SessionSubSystem.h"
#include "UI/Foundation/ShooterButtonBase.h"
#include "UI/OutGame/Room/ShooterRoomTypes.h"

namespace ShooterCreateRoomDialogLimits
{
	// 기존 MainMenuWidget 한계값과 같음
	constexpr int32 MinRoomPlayers = 2;
	constexpr int32 MaxRoomPlayers = 8;
}

void UShooterCreateRoomDialog::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Btn_Confirm->OnClicked().AddUObject(this, &ThisClass::HandleConfirmClicked);
	Btn_Cancel->OnClicked().AddUObject(this, &ThisClass::HandleCancelClicked);

	Slider_MaxPlayers->SetMinValue(ShooterCreateRoomDialogLimits::MinRoomPlayers);
	Slider_MaxPlayers->SetMaxValue(ShooterCreateRoomDialogLimits::MaxRoomPlayers);
	Slider_MaxPlayers->SetStepSize(1.0f);
	Slider_MaxPlayers->OnValueChanged.AddDynamic(this, &ThisClass::HandleMaxPlayersChanged);
}

void UShooterCreateRoomDialog::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 풀에서 재사용되므로 지난 입력·상태를 지움
	TextBox_RoomName->SetText(FText::GetEmpty());
	TextBox_RoomName->SetHintText(FText::FromString(
		FShooterRoomRules::SanitizeRoomName(FString(), GetLocalPlayerName())));
	Slider_MaxPlayers->SetValue(ShooterCreateRoomDialogLimits::MaxRoomPlayers);
	HandleMaxPlayersChanged(Slider_MaxPlayers->GetValue());
	SetBusy(false, FText::GetEmpty());

	if (USessionSubSystem* Session = GetSessionSubsystem())
	{
		Session->OnCreateRoomComplete.AddDynamic(this, &ThisClass::HandleCreateRoomComplete);
	}
}

void UShooterCreateRoomDialog::NativeOnDeactivated()
{
	if (USessionSubSystem* Session = GetSessionSubsystem())
	{
		Session->OnCreateRoomComplete.RemoveDynamic(this, &ThisClass::HandleCreateRoomComplete);
	}

	Super::NativeOnDeactivated();
}

UWidget* UShooterCreateRoomDialog::NativeGetDesiredFocusTarget() const
{
	return TextBox_RoomName;
}

bool UShooterCreateRoomDialog::NativeOnHandleBackAction()
{
	if (bIsBusy)
	{
		return true;
	}

	return Super::NativeOnHandleBackAction();
}

void UShooterCreateRoomDialog::HandleConfirmClicked()
{
	USessionSubSystem* Session = GetSessionSubsystem();
	if (!IsValid(Session) || bIsBusy)
	{
		return;
	}

	const FString RoomName = FShooterRoomRules::SanitizeRoomName(
		TextBox_RoomName->GetText().ToString(),
		GetLocalPlayerName());

	// 상태부터 바꾸고 호출. 실패는 CreateRoom 안에서 즉시 돌아옴
	SetBusy(true, NSLOCTEXT("ShooterLobby", "CreatingRoom", "방을 만드는 중"));
	Session->CreateRoom(GetSelectedMaxPlayers(), RoomName, TEXT("BombMap"), TEXT("Bomb"));
}

void UShooterCreateRoomDialog::HandleCancelClicked()
{
	if (bIsBusy)
	{
		return;
	}

	DeactivateWidget();
}

void UShooterCreateRoomDialog::HandleMaxPlayersChanged(float /*NewValue*/)
{
	Text_MaxPlayers->SetText(FText::AsNumber(GetSelectedMaxPlayers()));
}

void UShooterCreateRoomDialog::HandleCreateRoomComplete(const bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		return;
	}

	SetBusy(false, NSLOCTEXT("ShooterLobby", "CreateRoomFailed", "방을 만들지 못했습니다"));
}

USessionSubSystem* UShooterCreateRoomDialog::GetSessionSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return IsValid(GameInstance)
		? GameInstance->GetSubsystem<USessionSubSystem>()
		: nullptr;
}

FString UShooterCreateRoomDialog::GetLocalPlayerName() const
{
	const APlayerController* PlayerController = GetOwningPlayer();
	const APlayerState* PlayerState = IsValid(PlayerController) ? PlayerController->PlayerState.Get() : nullptr;
	return IsValid(PlayerState) ? PlayerState->GetPlayerName() : FString();
}

int32 UShooterCreateRoomDialog::GetSelectedMaxPlayers() const
{
	return FMath::Clamp(
		FMath::RoundToInt(Slider_MaxPlayers->GetValue()),
		ShooterCreateRoomDialogLimits::MinRoomPlayers,
		ShooterCreateRoomDialogLimits::MaxRoomPlayers);
}

void UShooterCreateRoomDialog::SetBusy(const bool bNewBusy, const FText& StatusText)
{
	// 취소·입력칸도 잠금을 위함
	bIsBusy = bNewBusy;

	TextBox_RoomName->SetIsEnabled(!bNewBusy);
	Slider_MaxPlayers->SetIsEnabled(!bNewBusy);
	Btn_Confirm->SetIsEnabled(!bNewBusy);
	Btn_Cancel->SetIsEnabled(!bNewBusy);

	Text_Status->SetText(StatusText);
	BP_ApplyStatus(!StatusText.IsEmpty());
}
