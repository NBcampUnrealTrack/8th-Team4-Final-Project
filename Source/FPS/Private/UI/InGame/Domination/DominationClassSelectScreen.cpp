#include "UI/InGame/Domination/DominationClassSelectScreen.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Player/DominationPlayerController.h"
#include "Player/DominationPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationClassSelectScreen, Log, All);

UDominationClassSelectScreen::UDominationClassSelectScreen()
{
	InputConfig = EShooterWidgetInputMode::Menu;

	FDominationClassCardPresentation& Trooper = ClassOptions.AddDefaulted_GetRef();
	Trooper.CombatClass = EDominationCombatClass::Trooper;
	Trooper.DisplayName = NSLOCTEXT(
		"DominationClassSelection",
		"TrooperName",
		"Trooper");
	Trooper.Description = NSLOCTEXT(
		"DominationClassSelection",
		"TrooperDescription",
		"Assault Rifle");

	FDominationClassCardPresentation& Ranger = ClassOptions.AddDefaulted_GetRef();
	Ranger.CombatClass = EDominationCombatClass::Ranger;
	Ranger.DisplayName = NSLOCTEXT(
		"DominationClassSelection",
		"RangerName",
		"Ranger");
	Ranger.Description = NSLOCTEXT(
		"DominationClassSelection",
		"RangerDescription",
		"Sniper Rifle");

	FDominationClassCardPresentation& Vanguard = ClassOptions.AddDefaulted_GetRef();
	Vanguard.CombatClass = EDominationCombatClass::Vanguard;
	Vanguard.DisplayName = NSLOCTEXT(
		"DominationClassSelection",
		"VanguardName",
		"Vanguard");
	Vanguard.Description = NSLOCTEXT(
		"DominationClassSelection",
		"VanguardDescription",
		"Shotgun");
}

void UDominationClassSelectScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	RebuildClassCards();
	BindToPlayerState();

	const EDominationCombatClass SelectedClass = BoundPlayerState.IsValid()
		? BoundPlayerState->GetCombatClass()
		: EDominationCombatClass::Unselected;
	RefreshSelectionState(SelectedClass);
}

void UDominationClassSelectScreen::NativeOnDeactivated()
{
	UnbindFromPlayerState();
	ClearClassCards();

	Super::NativeOnDeactivated();
}

void UDominationClassSelectScreen::HandleCombatClassChanged(
	const EDominationCombatClass SelectedClass)
{
	RefreshSelectionState(SelectedClass);
}

void UDominationClassSelectScreen::HandleClassCardClicked(
	const EDominationCombatClass RequestedClass)
{
	ADominationPlayerController* PlayerController =
		Cast<ADominationPlayerController>(GetOwningPlayer());
	if (!IsValid(PlayerController))
	{
		return;
	}

	PlayerController->SelectCombatClass(RequestedClass);
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

	for (const FDominationClassCardPresentation& Option : ClassOptions)
	{
		if (Option.CombatClass == EDominationCombatClass::Unselected)
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

		ClassCard->SetClassData(Option);
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
}

void UDominationClassSelectScreen::UnbindFromPlayerState()
{
	if (ADominationPlayerState* PlayerState = BoundPlayerState.Get())
	{
		PlayerState->OnCombatClassChanged.RemoveDynamic(
			this,
			&ThisClass::HandleCombatClassChanged);
	}

	BoundPlayerState.Reset();
}

void UDominationClassSelectScreen::RefreshSelectionState(
	const EDominationCombatClass SelectedClass)
{
	for (UDominationClassSelectCard* ClassCard : ClassCards)
	{
		if (IsValid(ClassCard))
		{
			ClassCard->SetSelectionState(SelectedClass);
		}
	}
}
