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

	if (IsValid(Image_WeaponArt))
	{
		if (UTexture2D* WeaponArtTexture = Presentation.WeaponArt.Get())
		{
			Image_WeaponArt->SetBrushFromTexture(WeaponArtTexture, true);
			Image_WeaponArt->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_WeaponArt->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UDominationClassSelectCard::SetSelectionState(
	const EDominationCombatClass PreviewClass,
	const bool bClassLocked)
{
	const bool bIsPreviewed = PreviewClass == CombatClass;

	SetIsEnabled(!bClassLocked);
	BP_UpdateSelectionPresentation(bIsPreviewed, bClassLocked);
}

void UDominationClassSelectCard::ApplyHoverState(const FDominationCardHoverState& State)
{
	SetRenderTranslation(State.Translation);
	SetRenderScale(FVector2D(State.Scale));
	SetRenderOpacity(State.Opacity);
}

EDominationCombatClass UDominationClassSelectCard::GetCombatClass() const
{
	return CombatClass;
}

void UDominationClassSelectCard::NativeOnClicked()
{
	Super::NativeOnClicked();

	if (CombatClass != EDominationCombatClass::Unselected)
	{
		OnClassCardClicked.Broadcast(CombatClass);
	}
}

void UDominationClassSelectCard::NativeOnHovered()
{
	Super::NativeOnHovered();

	if (CombatClass != EDominationCombatClass::Unselected)
	{
		OnClassCardHovered.Broadcast(CombatClass, true);
	}
}

void UDominationClassSelectCard::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();

	if (CombatClass != EDominationCombatClass::Unselected)
	{
		OnClassCardHovered.Broadcast(CombatClass, false);
	}
}
