#include "UI/InGame/Domination/DominationClassSelectCard.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UDominationClassSelectCard::SetClassData(
	const FDominationClassCardPresentation& Presentation)
{
	CombatClass = Presentation.CombatClass;

	if (IsValid(Text_ClassName))
	{
		Text_ClassName->SetText(Presentation.DisplayName);
	}

	if (IsValid(Text_Description))
	{
		Text_Description->SetText(Presentation.Description);
	}

	if (IsValid(Image_Icon))
	{
		if (UTexture2D* IconTexture = Presentation.Icon.LoadSynchronous())
		{
			Image_Icon->SetBrushFromTexture(IconTexture, true);
			Image_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_Icon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UDominationClassSelectCard::SetSelectionState(
	const EDominationCombatClass SelectedClass)
{
	const bool bSelectionLocked =
		SelectedClass != EDominationCombatClass::Unselected;
	const bool bIsSelected = bSelectionLocked && SelectedClass == CombatClass;

	SetIsEnabled(!bSelectionLocked);
	UpdateSelectionPresentation(bIsSelected, bSelectionLocked);
}

void UDominationClassSelectCard::NativeOnClicked()
{
	Super::NativeOnClicked();

	if (CombatClass != EDominationCombatClass::Unselected)
	{
		OnClassCardClicked.Broadcast(CombatClass);
	}
}
