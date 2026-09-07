#if WITH_EDITOR

#include "UI/Settings/ShooterOptionsScreen.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

namespace ShooterOptionsWbpGenerator
{
	// Dark glass palette. Neutral translucent surfaces carry the layout; cyan is only state/accent.
	static const FLinearColor CMain(0.004f, 0.015f, 0.017f, 0.90f);
	static const FLinearColor CPanel(0.012f, 0.032f, 0.034f, 0.64f);
	static const FLinearColor CRow(0.050f, 0.082f, 0.084f, 0.34f);
	static const FLinearColor CRowSelected(0.030f, 0.175f, 0.170f, 0.46f);
	static const FLinearColor CControl(0.006f, 0.025f, 0.027f, 0.78f);
	static const FLinearColor CAccent(0.030f, 0.900f, 0.820f, 1.00f);
	static const FLinearColor CAccentSoft(0.024f, 0.290f, 0.280f, 0.68f);
	static const FLinearColor CPrimaryText(0.970f, 0.990f, 0.990f, 1.00f);
	static const FLinearColor CSecondaryText(0.720f, 0.790f, 0.800f, 1.00f);
	static const FLinearColor CMutedText(0.500f, 0.610f, 0.620f, 1.00f);
	static const FLinearColor CInactiveSegment(0.145f, 0.220f, 0.225f, 0.78f);
	static constexpr int32 MaxChoiceSegments = 24;
	static constexpr float RowControlWidth = 340.0f;

	template <typename T>
	T* NewWidget(UWidgetTree* Tree, const FName Name)
	{
		return Tree->ConstructWidget<T>(T::StaticClass(), Name);
	}

	static FSlateChildSize FillSize(const float Value = 1.0f)
	{
		FSlateChildSize Size;
		Size.SizeRule = ESlateSizeRule::Fill;
		Size.Value = Value;
		return Size;
	}

	static FSlateChildSize AutoSize()
	{
		FSlateChildSize Size;
		Size.SizeRule = ESlateSizeRule::Automatic;
		Size.Value = 1.0f;
		return Size;
	}

	static UTextBlock* MakeText(
		UWidgetTree* Tree,
		const FName Name,
		const FString& Text,
		const int32 FontSize,
		const FLinearColor& Color = CPrimaryText,
		const ETextJustify::Type Justify = ETextJustify::Left,
		const bool bBold = false)
	{
		UTextBlock* Widget = NewWidget<UTextBlock>(Tree, Name);
		Widget->SetText(FText::FromString(Text));
		Widget->SetColorAndOpacity(FSlateColor(Color));
		Widget->SetJustification(Justify);
		FSlateFontInfo Font = Widget->GetFont();
		Font.Size = FontSize;
		if (bBold)
		{
			Font.TypefaceFontName = TEXT("Bold");
		}
		Widget->SetFont(Font);
		return Widget;
	}

	static UBorder* MakeBorder(UWidgetTree* Tree, const FName Name, const FLinearColor& Color, const FMargin Padding = FMargin(0.0f))
	{
		UBorder* Border = NewWidget<UBorder>(Tree, Name);
		Border->SetBrushColor(Color);
		Border->SetPadding(Padding);
		return Border;
	}


	static void ApplyFlatButtonStyle(
		UButton* Button,
		const FLinearColor& Normal,
		const FLinearColor& Hovered,
		const FLinearColor& Pressed)
	{
		if (!Button)
		{
			return;
		}

		FButtonStyle Style;
		Style.SetNormal(FSlateColorBrush(Normal));
		Style.SetHovered(FSlateColorBrush(Hovered));
		Style.SetPressed(FSlateColorBrush(Pressed));
		Style.SetDisabled(FSlateColorBrush(FLinearColor(Normal.R, Normal.G, Normal.B, Normal.A * 0.35f)));
		Style.SetNormalPadding(FMargin(0.0f));
		Style.SetPressedPadding(FMargin(0.0f));
		Button->SetStyle(Style);
		Button->SetBackgroundColor(FLinearColor::White);
		Button->SetColorAndOpacity(FLinearColor::White);
	}

	static UButton* MakeButton(
		UWidgetTree* Tree,
		const FName Name,
		const FString& Label,
		const int32 FontSize = 15,
		const FLinearColor& Background = CControl,
		const FLinearColor& TextColor = CPrimaryText)
	{
		UButton* Button = NewWidget<UButton>(Tree, Name);
		// Keep the Slate brushes neutral so SetBackgroundColor can be used at runtime
		// for selected/hovered state without bringing the default white-outline style back.
		ApplyFlatButtonStyle(
			Button,
			FLinearColor(1.0f, 1.0f, 1.0f, 0.72f),
			FLinearColor(1.0f, 1.0f, 1.0f, 0.88f),
			FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		Button->SetBackgroundColor(Background);

		UTextBlock* Text = MakeText(
			Tree,
			*FString::Printf(TEXT("TXT_%s"), *Name.ToString()),
			Label,
			FontSize,
			TextColor,
			ETextJustify::Center,
			false);
		Button->SetContent(Text);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Text->Slot))
		{
			Slot->SetPadding(FMargin(0.0f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Button;
	}

	static UButton* MakeInfoLabelButton(
		UWidgetTree* Tree,
		const FString& Prefix,
		const FString& Label)
	{
		UButton* Button = NewWidget<UButton>(Tree, *FString::Printf(TEXT("Btn_Info_%s"), *Prefix));
		ApplyFlatButtonStyle(
			Button,
			FLinearColor(0.0f, 0.0f, 0.0f, 0.0f),
			FLinearColor(1.0f, 1.0f, 1.0f, 0.025f),
			FLinearColor(1.0f, 1.0f, 1.0f, 0.045f));

		UTextBlock* Text = MakeText(
			Tree,
			*FString::Printf(TEXT("TXT_Label_%s"), *Prefix),
			Label,
			15,
			CPrimaryText,
			ETextJustify::Left,
			false);
		Button->SetContent(Text);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Text->Slot))
		{
			Slot->SetPadding(FMargin(0.0f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Button;
	}

	static void SetOverlayFill(UPanelSlot* BaseSlot, const FMargin Padding = FMargin(0.0f))
	{
		if (UOverlaySlot* Slot = Cast<UOverlaySlot>(BaseSlot))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
			Slot->SetPadding(Padding);
		}
	}

	static void SetOverlayCenter(UPanelSlot* BaseSlot)
	{
		if (UOverlaySlot* Slot = Cast<UOverlaySlot>(BaseSlot))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
	}

	static void SetHBoxFill(UPanelSlot* BaseSlot, const float Weight = 1.0f, const FMargin Padding = FMargin(0.0f), const EVerticalAlignment VAlign = VAlign_Fill)
	{
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(BaseSlot))
		{
			Slot->SetSize(FillSize(Weight));
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign);
		}
	}

	static void SetHBoxAuto(UPanelSlot* BaseSlot, const FMargin Padding = FMargin(0.0f), const EVerticalAlignment VAlign = VAlign_Fill)
	{
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(BaseSlot))
		{
			Slot->SetSize(AutoSize());
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign);
		}
	}

	static void SetVBoxFill(UPanelSlot* BaseSlot, const float Weight = 1.0f, const FMargin Padding = FMargin(0.0f))
	{
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(BaseSlot))
		{
			Slot->SetSize(FillSize(Weight));
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	static void SetVBoxAuto(UPanelSlot* BaseSlot, const FMargin Padding = FMargin(0.0f))
	{
		if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(BaseSlot))
		{
			Slot->SetSize(AutoSize());
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	static USpacer* AddSpacer(UWidgetTree* Tree, UVerticalBox* Parent, const float Height)
	{
		USpacer* Spacer = NewWidget<USpacer>(Tree, NAME_None);
		Spacer->SetSize(FVector2D(1.0f, Height));
		SetVBoxAuto(Parent->AddChild(Spacer));
		return Spacer;
	}

	static void AddSideSectionHeader(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Label)
	{
		UHorizontalBox* H = NewWidget<UHorizontalBox>(Tree, NAME_None);
		SetVBoxAuto(Parent->AddChild(H), FMargin(5.0f, 10.0f, 5.0f, 7.0f));

		USizeBox* AccentSize = NewWidget<USizeBox>(Tree, NAME_None);
		AccentSize->SetWidthOverride(26.0f);
		AccentSize->SetHeightOverride(2.0f);
		SetHBoxAuto(H->AddChild(AccentSize), FMargin(0.0f, 0.0f, 8.0f, 0.0f), VAlign_Center);
		AccentSize->SetContent(MakeBorder(Tree, NAME_None, CAccent));

		UTextBlock* Text = MakeText(Tree, NAME_None, Label, 10, CMutedText, ETextJustify::Left, true);
		SetHBoxFill(H->AddChild(Text), 1.0f, FMargin(0.0f), VAlign_Center);
	}


	static UTextBlock* AddSectionTitle(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Label)
	{
		UHorizontalBox* Header = NewWidget<UHorizontalBox>(Tree, NAME_None);
		SetVBoxAuto(Parent->AddChild(Header), FMargin(4.0f, 8.0f, 4.0f, 8.0f));

		USizeBox* AccentSize = NewWidget<USizeBox>(Tree, NAME_None);
		AccentSize->SetWidthOverride(54.0f);
		AccentSize->SetHeightOverride(2.0f);
		SetHBoxAuto(Header->AddChild(AccentSize), FMargin(0.0f, 0.0f, 10.0f, 0.0f), VAlign_Center);
		AccentSize->SetContent(MakeBorder(Tree, NAME_None, CAccent));

		UTextBlock* Title = MakeText(Tree, NAME_None, Label, 12, CSecondaryText, ETextJustify::Left, true);
		SetHBoxFill(Header->AddChild(Title), 1.0f, FMargin(0.0f), VAlign_Center);
		return Title;
	}

	static UBorder* MakeRowShell(UWidgetTree* Tree, UVerticalBox* Parent, const FName Name)
	{
		USizeBox* Height = NewWidget<USizeBox>(Tree, *FString::Printf(TEXT("SB_%s"), *Name.ToString()));
		Height->SetHeightOverride(48.0f);
		SetVBoxAuto(Parent->AddChild(Height), FMargin(0.0f, 0.0f, 0.0f, 4.0f));

		UBorder* Row = MakeBorder(Tree, Name, CRow, FMargin(14.0f, 0.0f, 10.0f, 0.0f));
		Height->SetContent(Row);
		return Row;
	}


	static void AddSelectorRow(
		UWidgetTree* Tree,
		UVerticalBox* Parent,
		const FString& Prefix,
		const FString& Label,
		const FString& InitialValue,
		const int32 InitialChoiceCount = 5,
		const int32 InitialChoiceIndex = 2)
	{
		UBorder* Row = MakeRowShell(Tree, Parent, *FString::Printf(TEXT("Border_Row_%s"), *Prefix));
		UHorizontalBox* H = NewWidget<UHorizontalBox>(Tree, *FString::Printf(TEXT("HB_Row_%s"), *Prefix));
		Row->SetContent(H);

		UButton* InfoButton = MakeInfoLabelButton(Tree, Prefix, Label);
		SetHBoxFill(H->AddChild(InfoButton), 1.0f, FMargin(0.0f), VAlign_Center);

		USizeBox* ControlSize = NewWidget<USizeBox>(Tree, *FString::Printf(TEXT("SB_Control_%s"), *Prefix));
		ControlSize->SetWidthOverride(RowControlWidth);
		ControlSize->SetHeightOverride(44.0f);
		SetHBoxAuto(H->AddChild(ControlSize), FMargin(14.0f, 0.0f, 0.0f, 0.0f), VAlign_Center);

		UBorder* Control = MakeBorder(
			Tree,
			*FString::Printf(TEXT("Border_Control_%s"), *Prefix),
			CControl,
			FMargin(4.0f, 2.0f));
		ControlSize->SetContent(Control);

		UVerticalBox* ControlV = NewWidget<UVerticalBox>(Tree, *FString::Printf(TEXT("VB_Control_%s"), *Prefix));
		Control->SetContent(ControlV);

		USizeBox* SelectorHeight = NewWidget<USizeBox>(Tree, NAME_None);
		SelectorHeight->SetHeightOverride(31.0f);
		SetVBoxAuto(ControlV->AddChild(SelectorHeight));

		UHorizontalBox* SelectorH = NewWidget<UHorizontalBox>(Tree, *FString::Printf(TEXT("HB_Selector_%s"), *Prefix));
		SelectorHeight->SetContent(SelectorH);

		auto AddArrow = [Tree, SelectorH, &Prefix](const TCHAR* Suffix, const TCHAR* Glyph)
		{
			USizeBox* ArrowSize = NewWidget<USizeBox>(Tree, NAME_None);
			ArrowSize->SetWidthOverride(38.0f);
			ArrowSize->SetHeightOverride(31.0f);
			SetHBoxAuto(SelectorH->AddChild(ArrowSize), FMargin(0.0f), VAlign_Center);
			const FName ArrowName = *FString::Printf(TEXT("Btn_%s%s"), *Prefix, Suffix);
			UButton* Arrow = MakeButton(
				Tree,
				ArrowName,
				Glyph,
				13,
				FLinearColor(0.012f, 0.055f, 0.058f, 0.86f),
				CAccent);
			// Roboto's angle-bracket baseline sits visually low. Shift only the glyph by one pixel;
			// the button hit box itself remains perfectly centered and symmetric.
			if (UTextBlock* ArrowText = Cast<UTextBlock>(Tree->FindWidget(*FString::Printf(TEXT("TXT_%s"), *ArrowName.ToString()))))
			{
				ArrowText->SetRenderTranslation(FVector2D(0.0f, -1.0f));
			}
			ArrowSize->SetContent(Arrow);
		};

		AddArrow(TEXT("Prev"), TEXT("<"));

		UTextBlock* Value = MakeText(
			Tree,
			*FString::Printf(TEXT("TXT_%sValue"), *Prefix),
			InitialValue,
			14,
			CPrimaryText,
			ETextJustify::Center,
			true);
		SetHBoxFill(SelectorH->AddChild(Value), 1.0f, FMargin(6.0f, 0.0f), VAlign_Center);

		AddArrow(TEXT("Next"), TEXT(">"));

		// A compact discrete-choice rail sits exactly below the central value column.
		USizeBox* SegmentHeight = NewWidget<USizeBox>(Tree, NAME_None);
		SegmentHeight->SetHeightOverride(5.0f);
		SetVBoxAuto(ControlV->AddChild(SegmentHeight), FMargin(0.0f, 4.0f, 0.0f, 0.0f));

		UHorizontalBox* SegmentRow = NewWidget<UHorizontalBox>(Tree, *FString::Printf(TEXT("HB_ChoiceSegmentRow_%s"), *Prefix));
		SegmentHeight->SetContent(SegmentRow);

		USizeBox* LeftInset = NewWidget<USizeBox>(Tree, NAME_None);
		LeftInset->SetWidthOverride(42.0f);
		SetHBoxAuto(SegmentRow->AddChild(LeftInset));

		UHorizontalBox* Segments = NewWidget<UHorizontalBox>(Tree, *FString::Printf(TEXT("HB_ChoiceSegments_%s"), *Prefix));
		SetHBoxFill(SegmentRow->AddChild(Segments), 1.0f, FMargin(8.0f, 0.0f), VAlign_Center);
		for (int32 Index = 0; Index < MaxChoiceSegments; ++Index)
		{
			USizeBox* SegmentSize = NewWidget<USizeBox>(Tree, *FString::Printf(TEXT("SB_Choice_%s_%d"), *Prefix, Index));
			SegmentSize->SetHeightOverride(4.0f);
			SegmentSize->SetVisibility(Index < InitialChoiceCount ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
			SetHBoxFill(
				Segments->AddChild(SegmentSize),
				1.0f,
				FMargin(2.5f, 0.0f, 2.5f, 0.0f),
				VAlign_Center);
			UBorder* Segment = MakeBorder(
				Tree,
				*FString::Printf(TEXT("Border_Choice_%s_%d"), *Prefix, Index),
				Index == InitialChoiceIndex ? CAccent : CInactiveSegment);
			SegmentSize->SetContent(Segment);
		}

		USizeBox* RightInset = NewWidget<USizeBox>(Tree, NAME_None);
		RightInset->SetWidthOverride(42.0f);
		SetHBoxAuto(SegmentRow->AddChild(RightInset));
	}


	static void AddWindowModeRow(UWidgetTree* Tree, UVerticalBox* Parent)
	{
		UBorder* Row = MakeRowShell(Tree, Parent, TEXT("Border_Row_WindowMode"));
		UHorizontalBox* H = NewWidget<UHorizontalBox>(Tree, TEXT("HB_Row_WindowMode"));
		Row->SetContent(H);

		UButton* InfoButton = MakeInfoLabelButton(Tree, TEXT("WindowMode"), TEXT("화면 모드"));
		SetHBoxFill(H->AddChild(InfoButton), 1.0f, FMargin(0.0f), VAlign_Center);

		USizeBox* ControlSize = NewWidget<USizeBox>(Tree, TEXT("SB_Control_WindowMode"));
		ControlSize->SetWidthOverride(RowControlWidth);
		ControlSize->SetHeightOverride(36.0f);
		SetHBoxAuto(H->AddChild(ControlSize), FMargin(14.0f, 0.0f, 0.0f, 0.0f), VAlign_Center);

		UHorizontalBox* Modes = NewWidget<UHorizontalBox>(Tree, TEXT("HB_WindowModeChoices"));
		ControlSize->SetContent(Modes);

		UButton* Fullscreen = MakeButton(Tree, TEXT("Btn_WindowModeFullscreen"), TEXT("전체 화면"), 13, CAccentSoft, CPrimaryText);
		SetHBoxFill(Modes->AddChild(Fullscreen), 1.0f, FMargin(0.0f, 0.0f, 3.0f, 0.0f), VAlign_Fill);

		UButton* Windowed = MakeButton(Tree, TEXT("Btn_WindowModeWindowed"), TEXT("창 모드"), 13, CControl, CPrimaryText);
		SetHBoxFill(Modes->AddChild(Windowed), 1.0f, FMargin(3.0f, 0.0f, 0.0f, 0.0f), VAlign_Fill);
	}


	static void AddToggleRow(
		UWidgetTree* Tree,
		UVerticalBox* Parent,
		const FString& Prefix,
		const FString& Label,
		const bool bInitial)
	{
		UBorder* Row = MakeRowShell(Tree, Parent, *FString::Printf(TEXT("Border_Row_%s"), *Prefix));
		UHorizontalBox* H = NewWidget<UHorizontalBox>(Tree, *FString::Printf(TEXT("HB_Row_%s"), *Prefix));
		Row->SetContent(H);

		UTextBlock* LabelText = MakeText(Tree, *FString::Printf(TEXT("TXT_Label_%s"), *Prefix), Label, 15, CPrimaryText);
		SetHBoxFill(H->AddChild(LabelText), 1.0f, FMargin(0.0f), VAlign_Center);

		USizeBox* ControlColumn = NewWidget<USizeBox>(Tree, *FString::Printf(TEXT("SB_ControlColumn_%s"), *Prefix));
		ControlColumn->SetWidthOverride(RowControlWidth);
		ControlColumn->SetHeightOverride(36.0f);
		SetHBoxAuto(H->AddChild(ControlColumn), FMargin(14.0f, 0.0f, 0.0f, 0.0f), VAlign_Center);

		UHorizontalBox* ControlH = NewWidget<UHorizontalBox>(Tree, NAME_None);
		ControlColumn->SetContent(ControlH);
		USpacer* PushRight = NewWidget<USpacer>(Tree, NAME_None);
		SetHBoxFill(ControlH->AddChild(PushRight), 1.0f);

		USizeBox* ToggleSize = NewWidget<USizeBox>(Tree, *FString::Printf(TEXT("SB_Control_%s"), *Prefix));
		ToggleSize->SetWidthOverride(126.0f);
		ToggleSize->SetHeightOverride(36.0f);
		SetHBoxAuto(ControlH->AddChild(ToggleSize), FMargin(0.0f), VAlign_Center);

		UButton* Toggle = NewWidget<UButton>(Tree, *FString::Printf(TEXT("Btn_%sToggle"), *Prefix));
		ApplyFlatButtonStyle(
			Toggle,
			FLinearColor(1.0f, 1.0f, 1.0f, 0.72f),
			FLinearColor(1.0f, 1.0f, 1.0f, 0.88f),
			FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		Toggle->SetBackgroundColor(bInitial ? CAccentSoft : CControl);
		UTextBlock* Value = MakeText(
			Tree,
			*FString::Printf(TEXT("TXT_%sValue"), *Prefix),
			bInitial ? TEXT("켜짐") : TEXT("꺼짐"),
			13,
			CPrimaryText,
			ETextJustify::Center,
			true);
		Toggle->SetContent(Value);
		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Value->Slot))
		{
			ButtonSlot->SetPadding(FMargin(0.0f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonSlot->SetVerticalAlignment(VAlign_Center);
		}
		ToggleSize->SetContent(Toggle);
	}


	static void AddSliderRow(
		UWidgetTree* Tree,
		UVerticalBox* Parent,
		const FString& Prefix,
		const FString& Label,
		const float Min,
		const float Max,
		const float Initial,
		const FString& InitialText)
	{
		UBorder* Row = MakeRowShell(Tree, Parent, *FString::Printf(TEXT("Border_Row_%s"), *Prefix));
		UHorizontalBox* H = NewWidget<UHorizontalBox>(Tree, *FString::Printf(TEXT("HB_Row_%s"), *Prefix));
		Row->SetContent(H);

		UTextBlock* LabelText = MakeText(Tree, *FString::Printf(TEXT("TXT_Label_%s"), *Prefix), Label, 15, CPrimaryText);
		SetHBoxFill(H->AddChild(LabelText), 1.0f, FMargin(0.0f), VAlign_Center);

		USizeBox* ControlColumn = NewWidget<USizeBox>(Tree, *FString::Printf(TEXT("SB_Control_%s"), *Prefix));
		ControlColumn->SetWidthOverride(RowControlWidth);
		ControlColumn->SetHeightOverride(36.0f);
		SetHBoxAuto(H->AddChild(ControlColumn), FMargin(14.0f, 0.0f, 0.0f, 0.0f), VAlign_Center);

		UHorizontalBox* ControlH = NewWidget<UHorizontalBox>(Tree, NAME_None);
		ControlColumn->SetContent(ControlH);

		USlider* Slider = NewWidget<USlider>(Tree, *FString::Printf(TEXT("SLD_%s"), *Prefix));
		Slider->SetMinValue(Min);
		Slider->SetMaxValue(Max);
		Slider->SetValue(Initial);
		Slider->SetSliderBarColor(FLinearColor(0.18f, 0.26f, 0.28f, 0.90f));
		Slider->SetSliderHandleColor(CAccent);
		SetHBoxFill(ControlH->AddChild(Slider), 1.0f, FMargin(8.0f, 0.0f, 14.0f, 0.0f), VAlign_Center);

		USizeBox* ValueSize = NewWidget<USizeBox>(Tree, NAME_None);
		ValueSize->SetWidthOverride(64.0f);
		SetHBoxAuto(ControlH->AddChild(ValueSize), FMargin(0.0f), VAlign_Center);
		UTextBlock* Value = MakeText(
			Tree,
			*FString::Printf(TEXT("TXT_%sValue"), *Prefix),
			InitialText,
			14,
			CPrimaryText,
			ETextJustify::Right,
			true);
		ValueSize->SetContent(Value);
	}


	static void AddActionRow(
		UWidgetTree* Tree,
		UVerticalBox* Parent,
		const FString& Label,
		const FString& ButtonName,
		const FString& ButtonLabel)
	{
		UBorder* Row = MakeRowShell(Tree, Parent, NAME_None);
		UHorizontalBox* H = NewWidget<UHorizontalBox>(Tree, NAME_None);
		Row->SetContent(H);

		UTextBlock* LabelText = MakeText(Tree, NAME_None, Label, 15, CPrimaryText);
		SetHBoxFill(H->AddChild(LabelText), 1.0f, FMargin(0.0f), VAlign_Center);

		USizeBox* ControlColumn = NewWidget<USizeBox>(Tree, NAME_None);
		ControlColumn->SetWidthOverride(RowControlWidth);
		ControlColumn->SetHeightOverride(36.0f);
		SetHBoxAuto(H->AddChild(ControlColumn), FMargin(14.0f, 0.0f, 0.0f, 0.0f), VAlign_Center);

		UHorizontalBox* ControlH = NewWidget<UHorizontalBox>(Tree, NAME_None);
		ControlColumn->SetContent(ControlH);
		USpacer* PushRight = NewWidget<USpacer>(Tree, NAME_None);
		SetHBoxFill(ControlH->AddChild(PushRight), 1.0f);

		USizeBox* ButtonSize = NewWidget<USizeBox>(Tree, NAME_None);
		ButtonSize->SetWidthOverride(190.0f);
		ButtonSize->SetHeightOverride(36.0f);
		SetHBoxAuto(ControlH->AddChild(ButtonSize), FMargin(0.0f), VAlign_Center);
		ButtonSize->SetContent(MakeButton(Tree, *ButtonName, ButtonLabel, 13, CAccentSoft, CPrimaryText));
	}


	static void AddMetricRow(
		UWidgetTree* Tree,
		UVerticalBox* Parent,
		const int32 Index,
		const FString& Label,
		const FString& Value)
	{
		USizeBox* Size = NewWidget<USizeBox>(Tree, *FString::Printf(TEXT("SB_ContextMetric_%d"), Index));
		Size->SetHeightOverride(48.0f);
		SetVBoxAuto(Parent->AddChild(Size), FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		UBorder* Card = MakeBorder(
			Tree,
			*FString::Printf(TEXT("Border_ContextMetric_%d"), Index),
			FLinearColor(0.020f, 0.030f, 0.035f, 0.62f),
			FMargin(10.0f, 6.0f));
		Size->SetContent(Card);
		UHorizontalBox* H = NewWidget<UHorizontalBox>(Tree, NAME_None);
		Card->SetContent(H);

		UTextBlock* L = MakeText(
			Tree,
			*FString::Printf(TEXT("TXT_ContextLabel%d"), Index),
			Label,
			11,
			CMutedText,
			ETextJustify::Left,
			true);
		SetHBoxFill(H->AddChild(L), 0.46f, FMargin(0.0f), VAlign_Center);

		UTextBlock* V = MakeText(
			Tree,
			*FString::Printf(TEXT("TXT_ContextValue%d"), Index),
			Value,
			13,
			CPrimaryText,
			ETextJustify::Right,
			true);
		SetHBoxFill(H->AddChild(V), 0.54f, FMargin(0.0f), VAlign_Center);
	}

	static UVerticalBox* MakePage(
		UWidgetTree* Tree,
		UWidgetSwitcher* Switcher,
		const FName PageName,
		const bool bShowScrollbar = false)
	{
		UBorder* PageBorder = MakeBorder(Tree, PageName, CPanel, FMargin(12.0f));
		Switcher->AddChild(PageBorder);
		UScrollBox* Scroll = NewWidget<UScrollBox>(Tree, *FString::Printf(TEXT("Scroll_%s"), *PageName.ToString()));
		Scroll->SetScrollBarVisibility(bShowScrollbar ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
		Scroll->SetScrollbarThickness(FVector2D(3.0f, 3.0f));
		Scroll->SetAnimateWheelScrolling(true);
		Scroll->SetWheelScrollMultiplier(48.0f);
		PageBorder->SetContent(Scroll);
		UVerticalBox* V = NewWidget<UVerticalBox>(Tree, *FString::Printf(TEXT("VB_%s"), *PageName.ToString()));
		if (UScrollBoxSlot* Slot = Cast<UScrollBoxSlot>(Scroll->AddChild(V)))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Top);
		}
		return V;
	}


	static UButton* AddSideButton(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Name, const FString& Label, const bool bSelected)
	{
		USizeBox* Row = NewWidget<USizeBox>(Tree, *FString::Printf(TEXT("SB_Side_%s"), *Name));
		Row->SetHeightOverride(42.0f);
		SetVBoxAuto(Parent->AddChild(Row), FMargin(0.0f, 0.0f, 0.0f, 4.0f));

		UOverlay* Overlay = NewWidget<UOverlay>(Tree, *FString::Printf(TEXT("OV_Side_%s"), *Name));
		Row->SetContent(Overlay);

		UButton* Button = MakeButton(
			Tree,
			*FString::Printf(TEXT("Btn_Side_%s"), *Name),
			Label,
			14,
			bSelected ? CAccentSoft : FLinearColor(0.010f, 0.030f, 0.032f, 0.48f),
			bSelected ? CPrimaryText : CSecondaryText);
		SetOverlayFill(Overlay->AddChild(Button));

		USizeBox* AccentSize = NewWidget<USizeBox>(Tree, *FString::Printf(TEXT("SB_SideAccent_%s"), *Name));
		AccentSize->SetWidthOverride(3.0f);
		AccentSize->SetVisibility(bSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (UOverlaySlot* Slot = Cast<UOverlaySlot>(Overlay->AddChild(AccentSize)))
		{
			Slot->SetHorizontalAlignment(HAlign_Left);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		AccentSize->SetContent(MakeBorder(Tree, *FString::Printf(TEXT("Border_SideAccent_%s"), *Name), CAccent));
		return Button;
	}

	static UButton* AddFooterButton(UWidgetTree* Tree, UHorizontalBox* Parent, const FString& Name, const FString& Label, const bool bPrimary)
	{
		USizeBox* Size = NewWidget<USizeBox>(Tree, *FString::Printf(TEXT("SB_%s"), *Name));
		Size->SetWidthOverride(180.0f);
		Size->SetHeightOverride(44.0f);
		SetHBoxAuto(Parent->AddChild(Size), FMargin(7.0f, 0.0f), VAlign_Center);

		// Footer actions use explicit Slate brushes per interaction state.
		// The previous implementation used a fixed BackgroundColor multiplied by
		// nearly-white brushes, so Normal/Hovered/Pressed looked almost identical.
		UButton* Button = NewWidget<UButton>(Tree, *Name);
		const FLinearColor NormalColor = bPrimary
			? FLinearColor(0.030f, 0.470f, 0.440f, 0.94f)
			: FLinearColor(0.010f, 0.040f, 0.043f, 0.88f);
		const FLinearColor HoverColor = bPrimary
			? FLinearColor(0.040f, 0.650f, 0.600f, 1.00f)
			: FLinearColor(0.025f, 0.210f, 0.205f, 0.96f);
		const FLinearColor PressedColor = bPrimary
			? FLinearColor(0.025f, 0.350f, 0.335f, 1.00f)
			: FLinearColor(0.030f, 0.300f, 0.285f, 1.00f);
		ApplyFlatButtonStyle(Button, NormalColor, HoverColor, PressedColor);
		Button->SetBackgroundColor(FLinearColor::White);

		UTextBlock* Text = MakeText(
			Tree,
			*FString::Printf(TEXT("TXT_%s"), *Name),
			Label,
			15,
			bPrimary ? FLinearColor(0.010f, 0.045f, 0.047f, 1.0f) : CPrimaryText,
			ETextJustify::Center,
			true);
		Button->SetContent(Text);
		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Text->Slot))
		{
			ButtonSlot->SetPadding(FMargin(0.0f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			ButtonSlot->SetVerticalAlignment(VAlign_Center);
		}

		Size->SetContent(Button);
		return Button;
	}

	static void AddHeaderStatusCard(
		UWidgetTree* Tree,
		UHorizontalBox* Parent,
		const FString& Prefix,
		const FString& Label,
		const FString& InitialValue,
		const float Width = 180.0f)
	{
		USizeBox* Size = NewWidget<USizeBox>(Tree, *FString::Printf(TEXT("SB_HeaderStatus_%s"), *Prefix));
		Size->SetWidthOverride(Width);
		Size->SetHeightOverride(46.0f);
		SetHBoxAuto(Parent->AddChild(Size), FMargin(5.0f, 0.0f), VAlign_Center);

		UBorder* Card = MakeBorder(
			Tree,
			*FString::Printf(TEXT("Border_HeaderStatus_%s"), *Prefix),
			FLinearColor(0.010f, 0.030f, 0.032f, 0.52f),
			FMargin(10.0f, 5.0f));
		Size->SetContent(Card);

		UVerticalBox* V = NewWidget<UVerticalBox>(Tree, NAME_None);
		Card->SetContent(V);
		SetVBoxAuto(V->AddChild(MakeText(
			Tree,
			*FString::Printf(TEXT("TXT_HeaderStatusLabel_%s"), *Prefix),
			Label,
			9,
			CMutedText,
			ETextJustify::Left,
			true)));
		SetVBoxAuto(V->AddChild(MakeText(
			Tree,
			*FString::Printf(TEXT("TXT_HeaderStatusValue_%s"), *Prefix),
			InitialValue,
			13,
			CPrimaryText,
			ETextJustify::Left,
			true)));
	}

	static void BuildWidgetTree(UWidgetBlueprint* Blueprint)
	{
		UWidgetTree* Tree = Blueprint->WidgetTree;
		check(Tree);

		UOverlay* Root = NewWidget<UOverlay>(Tree, TEXT("Overlay_Root"));
		Tree->RootWidget = Root;

		UBackgroundBlur* Blur = NewWidget<UBackgroundBlur>(Tree, TEXT("BackgroundBlur_BG"));
		Blur->SetBlurStrength(14.0f);
		Blur->SetApplyAlphaToBlur(true);
		SetOverlayFill(Root->AddChild(Blur));

		UBorder* Dim = MakeBorder(Tree, TEXT("Border_Dim"), FLinearColor(0.0f, 0.0f, 0.0f, 0.22f));
		SetOverlayFill(Root->AddChild(Dim));

		USizeBox* MainSize = NewWidget<USizeBox>(Tree, TEXT("SB_MainPanel"));
		MainSize->SetWidthOverride(1580.0f);
		MainSize->SetHeightOverride(900.0f);
		SetOverlayCenter(Root->AddChild(MainSize));

		UBorder* MainBorder = MakeBorder(Tree, TEXT("Border_MainGlassPanel"), CMain, FMargin(14.0f));
		MainSize->SetContent(MainBorder);

		UVerticalBox* Main = NewWidget<UVerticalBox>(Tree, TEXT("VB_MainLayout"));
		MainBorder->SetContent(Main);

		// Header: navigation lives in the left rail; the top area is an at-a-glance status strip.
		USizeBox* HeaderSize = NewWidget<USizeBox>(Tree, TEXT("SB_Header"));
		HeaderSize->SetHeightOverride(82.0f);
		SetVBoxAuto(Main->AddChild(HeaderSize), FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		UBorder* Header = MakeBorder(Tree, TEXT("Border_HeaderArea"), FLinearColor(0.008f, 0.025f, 0.027f, 0.50f), FMargin(14.0f, 8.0f));
		HeaderSize->SetContent(Header);

		UHorizontalBox* Top = NewWidget<UHorizontalBox>(Tree, TEXT("HB_TopBar"));
		Header->SetContent(Top);

		USizeBox* TitleSize = NewWidget<USizeBox>(Tree, TEXT("SB_HeaderTitle"));
		TitleSize->SetWidthOverride(330.0f);
		SetHBoxAuto(Top->AddChild(TitleSize), FMargin(0.0f, 0.0f, 20.0f, 0.0f), VAlign_Center);
		UVerticalBox* TitleV = NewWidget<UVerticalBox>(Tree, TEXT("VB_HeaderTitle"));
		TitleSize->SetContent(TitleV);
		SetVBoxAuto(TitleV->AddChild(MakeText(Tree, TEXT("TXT_HeaderEyebrow"), TEXT("GAME SETTINGS"), 10, CAccent, ETextJustify::Left, true)));
		SetVBoxAuto(TitleV->AddChild(MakeText(Tree, TEXT("TXT_HeaderTitle"), TEXT("게임 옵션"), 28, CPrimaryText, ETextJustify::Left, true)));

		USpacer* HeaderGap = NewWidget<USpacer>(Tree, NAME_None);
		SetHBoxFill(Top->AddChild(HeaderGap), 1.0f);

		AddHeaderStatusCard(Tree, Top, TEXT("Display"), TEXT("DISPLAY"), TEXT("1920 x 1080"), 190.0f);
		AddHeaderStatusCard(Tree, Top, TEXT("Quality"), TEXT("QUALITY"), TEXT("CUSTOM"), 170.0f);
		AddHeaderStatusCard(Tree, Top, TEXT("FPS"), TEXT("FRAME LIMIT"), TEXT("UNLIMITED"), 170.0f);

		// Body
		UHorizontalBox* Body = NewWidget<UHorizontalBox>(Tree, TEXT("HB_Body"));
		SetVBoxFill(Main->AddChild(Body), 1.0f, FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		USizeBox* LeftSize = NewWidget<USizeBox>(Tree, TEXT("SB_LeftSidebar"));
		LeftSize->SetWidthOverride(230.0f);
		SetHBoxAuto(Body->AddChild(LeftSize), FMargin(0.0f, 0.0f, 12.0f, 0.0f));
		UBorder* Left = MakeBorder(Tree, TEXT("Border_LeftSidebar"), FLinearColor(0.008f, 0.025f, 0.027f, 0.48f), FMargin(9.0f, 10.0f));
		LeftSize->SetContent(Left);
		UVerticalBox* Side = NewWidget<UVerticalBox>(Tree, TEXT("VB_LeftSidebarMenu"));
		Left->SetContent(Side);

		AddSideSectionHeader(Tree, Side, TEXT("화면"));
		AddSideButton(Tree, Side, TEXT("Graphics"), TEXT("그래픽 설정"), true);
		AddSideButton(Tree, Side, TEXT("Display"), TEXT("화면 설정"), false);

		AddSideSectionHeader(Tree, Side, TEXT("게임플레이"));
		AddSideButton(Tree, Side, TEXT("Controls"), TEXT("조작 설정"), false);

		AddSideSectionHeader(Tree, Side, TEXT("오디오"));
		AddSideButton(Tree, Side, TEXT("Audio"), TEXT("오디오 설정"), false);

		UWidgetSwitcher* Switcher = NewWidget<UWidgetSwitcher>(Tree, TEXT("Switcher_MainPages"));
		SetHBoxFill(Body->AddChild(Switcher), 1.0f, FMargin(0.0f, 0.0f, 12.0f, 0.0f));


		// Graphics and quality live on one scrollable page. The overall preset stays at the top,
		// while the detailed scalability rows continue below without clipping.
		UVerticalBox* Graphics = MakePage(Tree, Switcher, TEXT("Page_Graphics"), true);
		AddSectionTitle(Tree, Graphics, TEXT("GRAPHICS PRESET"));
		AddSelectorRow(Tree, Graphics, TEXT("Overall"), TEXT("전체 품질"), TEXT("HIGH"), 5, 2);
		AddSpacer(Tree, Graphics, 4.0f);
		AddSectionTitle(Tree, Graphics, TEXT("QUALITY"));
		AddSelectorRow(Tree, Graphics, TEXT("ViewDistance"), TEXT("시야 거리"), TEXT("HIGH"), 5, 2);
		AddSelectorRow(Tree, Graphics, TEXT("AntiAliasing"), TEXT("안티앨리어싱"), TEXT("HIGH"), 5, 2);
		AddSelectorRow(Tree, Graphics, TEXT("Shadow"), TEXT("그림자 품질"), TEXT("HIGH"), 5, 2);
		AddSelectorRow(Tree, Graphics, TEXT("GlobalIllumination"), TEXT("글로벌 일루미네이션"), TEXT("HIGH"), 5, 2);
		AddSelectorRow(Tree, Graphics, TEXT("Reflection"), TEXT("반사 품질"), TEXT("HIGH"), 5, 2);
		AddSelectorRow(Tree, Graphics, TEXT("PostProcessing"), TEXT("포스트 프로세싱"), TEXT("HIGH"), 5, 2);
		AddSelectorRow(Tree, Graphics, TEXT("Texture"), TEXT("텍스처 품질"), TEXT("HIGH"), 5, 2);
		AddSelectorRow(Tree, Graphics, TEXT("Effects"), TEXT("이펙트 품질"), TEXT("HIGH"), 5, 2);
		AddSelectorRow(Tree, Graphics, TEXT("Foliage"), TEXT("폴리지 품질"), TEXT("HIGH"), 5, 2);
		AddSelectorRow(Tree, Graphics, TEXT("Shading"), TEXT("셰이딩 품질"), TEXT("HIGH"), 5, 2);

		UVerticalBox* Video = MakePage(Tree, Switcher, TEXT("Page_Video"));
		AddSectionTitle(Tree, Video, TEXT("DISPLAY"));
		AddSelectorRow(Tree, Video, TEXT("Resolution"), TEXT("해상도"), TEXT("1920 x 1080"), 8, 4);
		AddWindowModeRow(Tree, Video);
		AddSliderRow(Tree, Video, TEXT("ResolutionScale"), TEXT("해상도 스케일"), 50.0f, 100.0f, 100.0f, TEXT("100%"));
		AddToggleRow(Tree, Video, TEXT("VSync"), TEXT("수직 동기화"), false);
		AddSelectorRow(Tree, Video, TEXT("FrameRate"), TEXT("FPS 제한"), TEXT("144"), 8, 4);
		AddToggleRow(Tree, Video, TEXT("DynamicResolution"), TEXT("다이내믹 레졸루션"), false);

		UVerticalBox* Controls = MakePage(Tree, Switcher, TEXT("Page_Controls"));
		AddSectionTitle(Tree, Controls, TEXT("LOOK"));
		AddSliderRow(Tree, Controls, TEXT("HorizontalSensitivity"), TEXT("수평 감도"), 0.01f, 10.0f, 1.0f, TEXT("1.00"));
		AddSliderRow(Tree, Controls, TEXT("VerticalSensitivity"), TEXT("수직 감도"), 0.01f, 10.0f, 1.0f, TEXT("1.00"));
		AddSliderRow(Tree, Controls, TEXT("AimSensitivity"), TEXT("ADS 감도 배율"), 0.01f, 2.0f, 1.0f, TEXT("1.00"));
		AddToggleRow(Tree, Controls, TEXT("InvertY"), TEXT("Y축 반전"), false);
		AddSectionTitle(Tree, Controls, TEXT("GAMEPLAY"));
		AddToggleRow(Tree, Controls, TEXT("DynamicCrosshair"), TEXT("동적 크로스헤어"), true);

		UVerticalBox* Audio = MakePage(Tree, Switcher, TEXT("Page_Audio"));
		AddSectionTitle(Tree, Audio, TEXT("VOLUME"));
		AddSliderRow(Tree, Audio, TEXT("MasterVolume"), TEXT("마스터"), 0.0f, 1.0f, 1.0f, TEXT("100"));
		AddSliderRow(Tree, Audio, TEXT("MusicVolume"), TEXT("음악"), 0.0f, 1.0f, 1.0f, TEXT("100"));
		AddSliderRow(Tree, Audio, TEXT("SfxVolume"), TEXT("효과음"), 0.0f, 1.0f, 1.0f, TEXT("100"));
		AddSliderRow(Tree, Audio, TEXT("UiVolume"), TEXT("UI"), 0.0f, 1.0f, 1.0f, TEXT("100"));
		AddSliderRow(Tree, Audio, TEXT("VoiceVolume"), TEXT("음성"), 0.0f, 1.0f, 1.0f, TEXT("100"));
		AddSectionTitle(Tree, Audio, TEXT("VOICE"));
		AddToggleRow(Tree, Audio, TEXT("MasterMute"), TEXT("전체 음소거"), false);
		AddToggleRow(Tree, Audio, TEXT("VoiceChat"), TEXT("음성 채팅"), true);

		Switcher->SetActiveWidgetIndex(0);

		// Context detail panel
		USizeBox* RightSize = NewWidget<USizeBox>(Tree, TEXT("SB_RightDetail"));
		RightSize->SetWidthOverride(390.0f);
		SetHBoxAuto(Body->AddChild(RightSize));
		UBorder* Right = MakeBorder(Tree, TEXT("Border_RightDetailPanel"), FLinearColor(0.008f, 0.025f, 0.027f, 0.54f), FMargin(16.0f));
		RightSize->SetContent(Right);
		UVerticalBox* Detail = NewWidget<UVerticalBox>(Tree, TEXT("VB_RightDetail"));
		Right->SetContent(Detail);

		SetVBoxAuto(Detail->AddChild(MakeText(Tree, TEXT("TXT_DetailCategory"), TEXT("GRAPHICS"), 10, CAccent, ETextJustify::Left, true)), FMargin(0.0f, 0.0f, 0.0f, 3.0f));
		SetVBoxAuto(Detail->AddChild(MakeText(Tree, TEXT("TXT_DetailTitle"), TEXT("전체 품질"), 25, CPrimaryText, ETextJustify::Left, true)), FMargin(0.0f, 0.0f, 0.0f, 5.0f));

		UBorder* CurrentBadge = MakeBorder(Tree, TEXT("Border_DetailCurrent"), FLinearColor(0.020f, 0.250f, 0.235f, 0.44f), FMargin(9.0f, 4.0f));
		SetVBoxAuto(Detail->AddChild(CurrentBadge), FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		CurrentBadge->SetContent(MakeText(Tree, TEXT("TXT_DetailCurrentValue"), TEXT("현재 HIGH"), 12, CPrimaryText, ETextJustify::Left, true));

		UTextBlock* DetailBody = MakeText(Tree, TEXT("TXT_DetailBody"), TEXT("여러 Scalability 항목을 하나의 프리셋으로 동시에 조절합니다."), 13, CSecondaryText);
		DetailBody->SetAutoWrapText(true);
		SetVBoxAuto(Detail->AddChild(DetailBody), FMargin(0.0f, 0.0f, 0.0f, 14.0f));

		UWidgetSwitcher* DetailVisual = NewWidget<UWidgetSwitcher>(Tree, TEXT("Switcher_DetailVisual"));
		SetVBoxAuto(Detail->AddChild(DetailVisual), FMargin(0.0f, 0.0f, 0.0f, 14.0f));

		// Graphics-only live scene card.
		UBorder* GraphicsVisual = MakeBorder(Tree, TEXT("Panel_GraphicsPreview"), FLinearColor(0.008f, 0.025f, 0.027f, 0.52f), FMargin(10.0f));
		DetailVisual->AddChild(GraphicsVisual);
		UVerticalBox* GraphicsVisualV = NewWidget<UVerticalBox>(Tree, TEXT("VB_GraphicsPreview"));
		GraphicsVisual->SetContent(GraphicsVisualV);
		SetVBoxAuto(GraphicsVisualV->AddChild(MakeText(Tree, TEXT("TXT_PreviewEyebrow"), TEXT("LIVE SCENE"), 10, CAccent, ETextJustify::Left, true)), FMargin(0.0f, 0.0f, 0.0f, 7.0f));

		USizeBox* PreviewSize = NewWidget<USizeBox>(Tree, TEXT("SB_Preview"));
		PreviewSize->SetHeightOverride(185.0f);
		SetVBoxAuto(GraphicsVisualV->AddChild(PreviewSize), FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		UBorder* PreviewFrame = MakeBorder(Tree, TEXT("Border_PreviewFrame"), FLinearColor(0.030f, 0.300f, 0.285f, 0.32f), FMargin(6.0f));
		PreviewSize->SetContent(PreviewFrame);
		UBorder* Preview = MakeBorder(Tree, TEXT("Border_Preview"), FLinearColor(0.006f, 0.010f, 0.012f, 1.0f), FMargin(0.0f));
		PreviewFrame->SetContent(Preview);
		UOverlay* PreviewOverlay = NewWidget<UOverlay>(Tree, TEXT("OV_Preview"));
		Preview->SetContent(PreviewOverlay);
		UImage* PreviewImage = NewWidget<UImage>(Tree, TEXT("IMG_Preview"));
		PreviewImage->SetVisibility(ESlateVisibility::Collapsed);
		SetOverlayFill(PreviewOverlay->AddChild(PreviewImage));
		UTextBlock* PreviewStatus = MakeText(
			Tree,
			TEXT("TXT_PreviewStatus"),
			TEXT("OptionsPreview 카메라 연결 대기 중"),
			12,
			CSecondaryText,
			ETextJustify::Center,
			true);
		PreviewStatus->SetAutoWrapText(true);
		SetOverlayCenter(PreviewOverlay->AddChild(PreviewStatus));

		SetVBoxAuto(GraphicsVisualV->AddChild(MakeText(Tree, TEXT("TXT_PerformanceTitle"), TEXT("예상 성능 영향도"), 11, CSecondaryText, ETextJustify::Left, true)), FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		UHorizontalBox* Perf = NewWidget<UHorizontalBox>(Tree, TEXT("HB_PerformanceBars"));
		SetVBoxAuto(GraphicsVisualV->AddChild(Perf));
		for (int32 i = 0; i < 6; ++i)
		{
			USizeBox* SegmentSize = NewWidget<USizeBox>(Tree, *FString::Printf(TEXT("SB_PerfSegment_%d"), i));
			SegmentSize->SetHeightOverride(6.0f);
			SetHBoxFill(Perf->AddChild(SegmentSize), 1.0f, FMargin(0.0f, 0.0f, i == 5 ? 0.0f : 4.0f, 0.0f), VAlign_Center);
			SegmentSize->SetContent(MakeBorder(
				Tree,
				*FString::Printf(TEXT("Border_PerfSegment_%d"), i),
				i < 3 ? CAccent : CInactiveSegment));
		}

		// Video / Controls / Audio contextual data card: no irrelevant SceneCapture image.
		UBorder* ContextVisual = MakeBorder(Tree, TEXT("Panel_ContextInfo"), FLinearColor(0.008f, 0.025f, 0.027f, 0.52f), FMargin(10.0f));
		DetailVisual->AddChild(ContextVisual);
		UVerticalBox* ContextV = NewWidget<UVerticalBox>(Tree, TEXT("VB_ContextInfo"));
		ContextVisual->SetContent(ContextV);
		SetVBoxAuto(ContextV->AddChild(MakeText(Tree, TEXT("TXT_ContextEyebrow"), TEXT("SETTING INFO"), 10, CAccent, ETextJustify::Left, true)), FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		AddMetricRow(Tree, ContextV, 0, TEXT("현재 값"), TEXT("-"));
		AddMetricRow(Tree, ContextV, 1, TEXT("범위 / 선택"), TEXT("-"));
		AddMetricRow(Tree, ContextV, 2, TEXT("적용"), TEXT("-"));

		DetailVisual->SetActiveWidgetIndex(0);

		// Footer
		USizeBox* FooterSize = NewWidget<USizeBox>(Tree, TEXT("SB_Footer"));
		FooterSize->SetHeightOverride(58.0f);
		SetVBoxAuto(Main->AddChild(FooterSize));
		UBorder* FooterBorder = MakeBorder(Tree, TEXT("Border_Footer"), FLinearColor(0.006f, 0.020f, 0.022f, 0.50f), FMargin(7.0f));
		FooterSize->SetContent(FooterBorder);
		UHorizontalBox* Footer = NewWidget<UHorizontalBox>(Tree, TEXT("HB_Footer"));
		FooterBorder->SetContent(Footer);
		USpacer* FooterLeft = NewWidget<USpacer>(Tree, NAME_None);
		SetHBoxFill(Footer->AddChild(FooterLeft), 1.0f);
		AddFooterButton(Tree, Footer, TEXT("Btn_Reset"), TEXT("초기화"), false);
		AddFooterButton(Tree, Footer, TEXT("Btn_Save"), TEXT("저장"), true);
		AddFooterButton(Tree, Footer, TEXT("Btn_Back"), TEXT("뒤로"), false);
		USpacer* FooterRight = NewWidget<USpacer>(Tree, NAME_None);
		SetHBoxFill(Footer->AddChild(FooterRight), 1.0f);

		// Video confirmation popup
		UOverlay* Popup = NewWidget<UOverlay>(Tree, TEXT("Overlay_VideoConfirm"));
		Popup->SetVisibility(ESlateVisibility::Collapsed);
		SetOverlayFill(Root->AddChild(Popup));
		UBorder* PopupDim = MakeBorder(Tree, TEXT("Border_VideoConfirmDim"), FLinearColor(0.0f, 0.0f, 0.0f, 0.66f));
		SetOverlayFill(Popup->AddChild(PopupDim));
		USizeBox* PopupSize = NewWidget<USizeBox>(Tree, TEXT("SB_VideoConfirm"));
		PopupSize->SetWidthOverride(520.0f);
		PopupSize->SetHeightOverride(245.0f);
		SetOverlayCenter(Popup->AddChild(PopupSize));
		UBorder* PopupPanel = MakeBorder(Tree, TEXT("Border_VideoConfirmPanel"), FLinearColor(0.020f, 0.030f, 0.035f, 0.96f), FMargin(24.0f));
		PopupSize->SetContent(PopupPanel);
		UVerticalBox* PopupV = NewWidget<UVerticalBox>(Tree, TEXT("VB_VideoConfirm"));
		PopupPanel->SetContent(PopupV);
		SetVBoxAuto(PopupV->AddChild(MakeText(Tree, TEXT("TXT_VideoConfirmTitle"), TEXT("변경한 화면 설정을 유지하시겠습니까?"), 20, CPrimaryText, ETextJustify::Center, true)), FMargin(0.0f, 8.0f, 0.0f, 10.0f));
		SetVBoxAuto(PopupV->AddChild(MakeText(Tree, TEXT("TXT_VideoConfirmCountdown"), TEXT("15초 후 이전 설정으로 복구됩니다."), 13, CSecondaryText, ETextJustify::Center)), FMargin(0.0f, 0.0f, 0.0f, 24.0f));
		UHorizontalBox* PopupButtons = NewWidget<UHorizontalBox>(Tree, TEXT("HB_VideoConfirmButtons"));
		SetVBoxAuto(PopupV->AddChild(PopupButtons));
		USpacer* PLeft = NewWidget<USpacer>(Tree, NAME_None);
		SetHBoxFill(PopupButtons->AddChild(PLeft), 1.0f);
		AddFooterButton(Tree, PopupButtons, TEXT("Btn_VideoKeep"), TEXT("유지"), true);
		AddFooterButton(Tree, PopupButtons, TEXT("Btn_VideoRevert"), TEXT("취소"), false);
		USpacer* PRight = NewWidget<USpacer>(Tree, NAME_None);
		SetHBoxFill(PopupButtons->AddChild(PRight), 1.0f);
	}


	static void Generate()
	{
		const FString PackagePath = TEXT("/Game/UI/Settings");
		const FString AssetName = TEXT("WBP_OptionsScreen_Auto");
		const FString LongPackageName = PackagePath + TEXT("/") + AssetName;
		const FString ObjectPath = LongPackageName + TEXT(".") + AssetName;

		if (FindObject<UObject>(nullptr, *ObjectPath) || FPackageName::DoesPackageExist(LongPackageName))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
				TEXT("WBP_OptionsScreen_Auto가 이미 존재합니다. 기존 에셋을 지우거나 이름을 바꾼 뒤 다시 실행하세요.")));
			return;
		}

		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = UShooterOptionsScreen::StaticClass();
		Factory->BlueprintType = BPTYPE_Normal;

		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, UWidgetBlueprint::StaticClass(), Factory);
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(NewAsset);
		if (!Blueprint || !Blueprint->WidgetTree)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Widget Blueprint 생성에 실패했습니다.")));
			return;
		}

		if (Blueprint->WidgetTree->RootWidget)
		{
			Blueprint->WidgetTree->RemoveWidget(Blueprint->WidgetTree->RootWidget);
			Blueprint->WidgetTree->RootWidget = nullptr;
		}

		BuildWidgetTree(Blueprint);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		Blueprint->MarkPackageDirty();

		UPackage* Package = Blueprint->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		UPackage::SavePackage(Package, Blueprint, *Filename, SaveArgs);

		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
			TEXT("생성 완료: /Game/UI/Settings/WBP_OptionsScreen_Auto\n\nOptions Screen Class에 이 WBP를 지정하면 됩니다.")));
	}

	static FAutoConsoleCommand GenerateCommand(
		TEXT("FPS.UI.GenerateOptionsWBP"),
		TEXT("Creates /Game/UI/Settings/WBP_OptionsScreen_Auto using ShooterOptionsScreen as the parent class."),
		FConsoleCommandDelegate::CreateStatic(&Generate));
}

#endif
