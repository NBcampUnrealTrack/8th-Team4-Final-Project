#include "UI/InGame/Domination/DominationClassSelectScreen.h"

#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Engine/World.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "GameMode/State/DominationGameState.h"
#include "Player/DominationPlayerController.h"
#include "Player/DominationPlayerState.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationClassSelectScreen, Log, All);

UDominationClassSelectScreen::UDominationClassSelectScreen()
{
	InputConfig = EShooterWidgetInputMode::Menu;
}

void UDominationClassSelectScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	RebuildClassCards();
	BindToPlayerState();

	if (IsValid(Button_LockIn))
	{
		Button_LockIn->OnClicked().AddUObject(this, &ThisClass::HandleLockInClicked);
	}

	RefreshTimerText();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TimerTextHandle,
			this,
			&ThisClass::RefreshTimerText,
			0.10f,
			true);
	}
}

void UDominationClassSelectScreen::NativeOnDeactivated()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerTextHandle);
	}

	if (IsValid(Button_LockIn))
	{
		Button_LockIn->OnClicked().RemoveAll(this);
	}

	bLockInRequestPending = false;

	UnbindFromPlayerState();
	ClearClassCards();

	Super::NativeOnDeactivated();
}

void UDominationClassSelectScreen::HandleCombatClassChanged(
	const EDominationCombatClass SelectedClass)
{
	RefreshSelectionState();
	RefreshLockInButton();
}

void UDominationClassSelectScreen::HandlePreviewCombatClassChanged(
	const EDominationCombatClass PreviewClass)
{
	RefreshSelectionState();
	RefreshLockInButton();
}

void UDominationClassSelectScreen::HandleClassCardClicked(
	const EDominationCombatClass RequestedClass)
{
	// 확정 요청을 보낸 뒤에는 임시 선택도 바꾸지 못하게 함
	if (bLockInRequestPending)
	{
		return;
	}

	ADominationPlayerController* PlayerController = Cast<ADominationPlayerController>(GetOwningPlayer());
	if (!IsValid(PlayerController))
	{
		return;
	}

	PlayerController->SetPreviewCombatClass(RequestedClass);
}

void UDominationClassSelectScreen::HandleLockInClicked()
{
	if (bLockInRequestPending)
	{
		return;
	}

	const ADominationPlayerState* PlayerState = BoundPlayerState.Get();
	ADominationPlayerController* PlayerController = Cast<ADominationPlayerController>(GetOwningPlayer());
	if (!IsValid(PlayerState)
		|| !IsValid(PlayerController)
		|| !PlayerState->HasPreviewCombatClass())
	{
		return;
	}

	bLockInRequestPending = true;
	RefreshLockInButton();

	PlayerController->SelectCombatClass(PlayerState->GetPreviewCombatClass());
}

void UDominationClassSelectScreen::RebuildClassCards()
{
	ClearClassCards();

	if (!IsValid(Panel_ClassCards) || !ClassCardWidgetClass)
	{
		UE_LOG(LogDominationClassSelectScreen, Error,
			TEXT("Class selection UI is not configured on %s."),
			*GetNameSafe(this));
		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	if (!IsValid(PlayerController))
	{
		return;
	}

	const UDominationClassSettings* ClassSettings =
		GetDefault<UDominationClassSettings>();
	if (!IsValid(ClassSettings))
	{
		UE_LOG(LogDominationClassSelectScreen, Error,
			TEXT("Domination class settings are unavailable."));
		return;
	}

	for (const FDominationCombatClassDefinition& Definition : ClassSettings->CombatClasses)
	{
		if (Definition.CombatClass == EDominationCombatClass::Unselected)
		{
			continue;
		}

		UDominationClassSelectCard* ClassCard =
			CreateWidget<UDominationClassSelectCard>(
				PlayerController,
				ClassCardWidgetClass);
		if (!IsValid(ClassCard))
		{
			continue;
		}

		FDominationClassCardPresentation Presentation;
		Presentation.CombatClass = Definition.CombatClass;
		Presentation.DisplayName = Definition.DisplayName;
		Presentation.Description = Definition.Description;
		Presentation.Icon = Definition.Icon;
		ClassCard->SetClassData(Presentation);
		ClassCard->OnClassCardClicked.AddUObject(
			this,
			&ThisClass::HandleClassCardClicked);

		UHorizontalBoxSlot* ClassCardSlot =
			Panel_ClassCards->AddChildToHorizontalBox(ClassCard);

		if (IsValid(ClassCardSlot))
		{
			ClassCardSlot->SetPadding(ClassCardPadding);
			ClassCardSlot->SetHorizontalAlignment(HAlign_Center);
			ClassCardSlot->SetVerticalAlignment(VAlign_Center);
		}

		ClassCards.Add(ClassCard);
	}
}

void UDominationClassSelectScreen::ClearClassCards()
{
	for (UDominationClassSelectCard* ClassCard : ClassCards)
	{
		if (IsValid(ClassCard))
		{
			ClassCard->OnClassCardClicked.RemoveAll(this);
		}
	}

	ClassCards.Reset();
	if (IsValid(Panel_ClassCards))
	{
		Panel_ClassCards->ClearChildren();
	}
}

void UDominationClassSelectScreen::BindToPlayerState()
{
	UnbindFromPlayerState();

	APlayerController* PlayerController = GetOwningPlayer();
	ADominationPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ADominationPlayerState>()
		: nullptr;
	if (!IsValid(PlayerState))
	{
		return;
	}

	BoundPlayerState = PlayerState;
	PlayerState->OnCombatClassChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleCombatClassChanged);
	PlayerState->OnPreviewCombatClassChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandlePreviewCombatClassChanged);

	// 이미 복제되어 있을 수 있으므로 직접 갱신
	RefreshSelectionState();
	RefreshLockInButton();
}

void UDominationClassSelectScreen::UnbindFromPlayerState()
{
	if (ADominationPlayerState* PlayerState = BoundPlayerState.Get())
	{
		PlayerState->OnCombatClassChanged.RemoveDynamic(
			this,
			&ThisClass::HandleCombatClassChanged);
		PlayerState->OnPreviewCombatClassChanged.RemoveDynamic(
			this,
			&ThisClass::HandlePreviewCombatClassChanged);
	}

	BoundPlayerState.Reset();
}

void UDominationClassSelectScreen::RefreshSelectionState()
{
	const ADominationPlayerState* PlayerState = BoundPlayerState.Get();
	const EDominationCombatClass PreviewClass = IsValid(PlayerState)
		? PlayerState->GetPreviewCombatClass()
		: EDominationCombatClass::Unselected;
	const bool bLocked = IsValid(PlayerState) && PlayerState->HasSelectedCombatClass();

	for (UDominationClassSelectCard* ClassCard : ClassCards)
	{
		if (IsValid(ClassCard))
		{
			ClassCard->SetSelectionState(PreviewClass, bLocked);
		}
	}
}

void UDominationClassSelectScreen::RefreshLockInButton()
{
	const ADominationPlayerState* PlayerState = BoundPlayerState.Get();
	const bool bLocked = IsValid(PlayerState) && PlayerState->HasSelectedCombatClass();
	const bool bHasPreview = IsValid(PlayerState) && PlayerState->HasPreviewCombatClass();

	if (IsValid(Button_LockIn))
	{
		// Preview 유무만 보면 확정 직후에도 버튼이 살아 있다.
		Button_LockIn->SetIsEnabled(!bLocked && bHasPreview && !bLockInRequestPending);
	}

	UpdateLockInPresentation(bLocked, bHasPreview, bLockInRequestPending);
}

void UDominationClassSelectScreen::RefreshTimerText()
{
	if (!IsValid(Text_Timer))
	{
		return;
	}

	const UWorld* World = GetWorld();
	const ADominationGameState* DominationGameState = IsValid(World)
		? World->GetGameState<ADominationGameState>()
		: nullptr;
	if (!IsValid(DominationGameState))
	{
		Text_Timer->SetText(FText::GetEmpty());
		return;
	}

	const int32 TotalSeconds =
		FMath::Max(0, FMath::CeilToInt(DominationGameState->GetPhaseTimeRemaining()));
	Text_Timer->SetText(FText::FromString(
		FString::Printf(TEXT("%02d:%02d"), TotalSeconds / 60, TotalSeconds % 60)));
}
