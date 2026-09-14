#include "UI/InGame/Domination/DominationClassInfoPanel.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "UI/InGame/Domination/DominationClassTraitTag.h"

void UDominationClassInfoPanel::SetDisplayClass(
	const FDominationCombatClassDefinition* Definition,
	UTexture2D* WeaponIcon)
{
	// 고르지않았으면 숨김
	if (Definition == nullptr)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (IsValid(Text_ClassName))
	{
		Text_ClassName->SetText(Definition->DisplayName);
	}

	if (IsValid(Image_Emblem))
	{
		if (IsValid(WeaponIcon))
		{
			Image_Emblem->SetBrushFromTexture(WeaponIcon, true);
			Image_Emblem->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_Emblem->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (IsValid(Text_PrimaryWeapon))
	{
		// 주무기명은 클래스 정의의 Description이 담고 있다. 현재 클래스와 주무기가 1:1이다.
		const bool bHasWeaponName = !Definition->Description.IsEmpty();
		if (bHasWeaponName)
		{
			Text_PrimaryWeapon->SetText(Definition->Description);
		}

		Text_PrimaryWeapon->SetVisibility(bHasWeaponName
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	RebuildTraitTags(Definition->TraitTags);
}

void UDominationClassInfoPanel::RebuildTraitTags(const TArray<FText>& TraitTags)
{
	if (!IsValid(Panel_TraitTags))
	{
		return;
	}

	Panel_TraitTags->ClearChildren();

	if (TraitTags.IsEmpty() || !TraitTagWidgetClass)
	{
		Panel_TraitTags->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Panel_TraitTags->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	for (const FText& TraitText : TraitTags)
	{
		UDominationClassTraitTag* TraitWidget =
			CreateWidget<UDominationClassTraitTag>(this, TraitTagWidgetClass);
		if (!IsValid(TraitWidget))
		{
			continue;
		}

		TraitWidget->SetTraitText(TraitText);
		Panel_TraitTags->AddChildToVerticalBox(TraitWidget);
	}
}
