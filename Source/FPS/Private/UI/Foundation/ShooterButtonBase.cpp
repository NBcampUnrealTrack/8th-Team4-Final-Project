// ShooterButtonBase.cpp
#include "UI/Foundation/ShooterButtonBase.h"

#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/TextBlock.h"

void UShooterButtonBase::SetButtonText(const FText& InText){
	ButtonText = InText;

	UpdateButtonText(ButtonText);
}

void UShooterButtonBase::NativePreConstruct(){
	Super::NativePreConstruct();

	UpdateButtonText(ButtonText);
	ApplyCurrentTextColor();
}

void UShooterButtonBase::NativeOnCurrentTextStyleChanged(){
	Super::NativeOnCurrentTextStyleChanged();

	ApplyCurrentTextColor();
}

void UShooterButtonBase::ApplyCurrentTextColor(){
	const UCommonTextStyle* TextStyle = GetCurrentTextStyle();
	if (!IsValid(TextStyle) || !WidgetTree)
	{
		return;
	}

	FLinearColor TextColor;
	TextStyle->GetColor(TextColor);

	WidgetTree->ForEachWidget([&TextColor](UWidget* Widget)
	{
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
		{
			TextBlock->SetColorAndOpacity(TextColor);
		}
	});
}

