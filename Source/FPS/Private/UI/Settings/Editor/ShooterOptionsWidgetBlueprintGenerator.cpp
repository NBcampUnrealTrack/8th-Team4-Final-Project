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
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "IAssetTools.h"
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
		// Widgets created in a fresh WidgetTree do not need per-widget Modify() calls.
		// Keeping creation side-effect free avoids editor transaction/reinstancing work while
		// the hierarchy is still being assembled.
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

	static void AddSideSectionHeader(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Key, const FString& Label)
	{
		UHorizontalBox* H = NewWidget<UHorizontalBox>(Tree, NAME_None);
		SetVBoxAuto(Parent->AddChild(H), FMargin(5.0f, 10.0f, 5.0f, 7.0f));

		USizeBox* AccentSize = NewWidget<USizeBox>(Tree, NAME_None);
		AccentSize->SetWidthOverride(26.0f);
		AccentSize->SetHeightOverride(2.0f);
		SetHBoxAuto(H->AddChild(AccentSize), FMargin(0.0f, 0.0f, 8.0f, 0.0f), VAlign_Center);
		AccentSize->SetContent(MakeBorder(Tree, NAME_None, CAccent));

		UTextBlock* Text = MakeText(Tree, *FString::Printf(TEXT("TXT_SideSection_%s"), *Key), Label, 10, CMutedText, ETextJustify::Left, true);
		SetHBoxFill(H->AddChild(Text), 1.0f, FMargin(0.0f), VAlign_Center);
	}


	static UTextBlock* AddSectionTitle(UWidgetTree* Tree, UVerticalBox* Parent, const FString& Key, const FString& Label)
	{
		UHorizontalBox* Header = NewWidget<UHorizontalBox>(Tree, NAME_None);
		SetVBoxAuto(Parent->AddChild(Header), FMargin(4.0f, 8.0f, 4.0f, 8.0f));

		USizeBox* AccentSize = NewWidget<USizeBox>(Tree, NAME_None);
		AccentSize->SetWidthOverride(54.0f);
		AccentSize->SetHeightOverride(2.0f);
		SetHBoxAuto(Header->AddChild(AccentSize), FMargin(0.0f, 0.0f, 10.0f, 0.0f), VAlign_Center);
		AccentSize->SetContent(MakeBorder(Tree, NAME_None, CAccent));

		UTextBlock* Title = MakeText(Tree, *FString::Printf(TEXT("TXT_Section_%s"), *Key), Label, 12, CSecondaryText, ETextJustify::Left, true);
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
		UBorder* Row = MakeRowShell(Tree, Parent, *FString::Printf(TEXT("Border_Row_%s"), *ButtonName));
		UHorizontalBox* H = NewWidget<UHorizontalBox>(Tree, NAME_None);
		Row->SetContent(H);

		UTextBlock* LabelText = MakeText(Tree, *FString::Printf(TEXT("TXT_Label_%s"), *ButtonName), Label, 15, CPrimaryText);
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
		AddHeaderStatusCard(Tree, Top, TEXT("Target"), TEXT("TARGET"), TEXT("120 FPS"), 150.0f);

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

		AddSideSectionHeader(Tree, Side, TEXT("Display"), TEXT("화면"));
		AddSideButton(Tree, Side, TEXT("Graphics"), TEXT("그래픽 설정"), true);
		AddSideButton(Tree, Side, TEXT("Display"), TEXT("화면 설정"), false);

		AddSideSectionHeader(Tree, Side, TEXT("Gameplay"), TEXT("게임플레이"));
		AddSideButton(Tree, Side, TEXT("Controls"), TEXT("조작 설정"), false);

		AddSideSectionHeader(Tree, Side, TEXT("Audio"), TEXT("오디오"));
		AddSideButton(Tree, Side, TEXT("Audio"), TEXT("오디오 설정"), false);

		AddSideSectionHeader(Tree, Side, TEXT("Options"), TEXT("설정"));
		AddSideButton(Tree, Side, TEXT("Settings"), TEXT("옵션 설정"), false);

		AddSideSectionHeader(Tree, Side, TEXT("Smart"), TEXT("스마트 도구"));
		AddSideButton(Tree, Side, TEXT("SmartTuning"), TEXT("스마트 튜닝"), false);

		UWidgetSwitcher* Switcher = NewWidget<UWidgetSwitcher>(Tree, TEXT("Switcher_MainPages"));
		SetHBoxFill(Body->AddChild(Switcher), 1.0f, FMargin(0.0f, 0.0f, 12.0f, 0.0f));


		// Graphics and quality live on one scrollable page. The overall preset stays at the top,
		// while the detailed scalability rows continue below without clipping.
		UVerticalBox* Graphics = MakePage(Tree, Switcher, TEXT("Page_Graphics"), true);
		AddSectionTitle(Tree, Graphics, TEXT("GraphicsPreset"), TEXT("GRAPHICS PRESET"));
		AddSelectorRow(Tree, Graphics, TEXT("Overall"), TEXT("전체 품질"), TEXT("HIGH"), 5, 2);
		AddSpacer(Tree, Graphics, 4.0f);
		AddSectionTitle(Tree, Graphics, TEXT("Quality"), TEXT("QUALITY"));
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
		AddSectionTitle(Tree, Video, TEXT("Display"), TEXT("DISPLAY"));
		AddSelectorRow(Tree, Video, TEXT("Resolution"), TEXT("해상도"), TEXT("1920 x 1080"), 8, 4);
		AddWindowModeRow(Tree, Video);
		AddSliderRow(Tree, Video, TEXT("ResolutionScale"), TEXT("해상도 스케일"), 50.0f, 100.0f, 100.0f, TEXT("100%"));
		AddToggleRow(Tree, Video, TEXT("VSync"), TEXT("수직 동기화"), false);
		AddSelectorRow(Tree, Video, TEXT("FrameRate"), TEXT("FPS 제한"), TEXT("144"), 8, 4);
		AddToggleRow(Tree, Video, TEXT("DynamicResolution"), TEXT("다이내믹 레졸루션"), false);

		UVerticalBox* Controls = MakePage(Tree, Switcher, TEXT("Page_Controls"));
		AddSectionTitle(Tree, Controls, TEXT("Look"), TEXT("LOOK"));
		AddSliderRow(Tree, Controls, TEXT("HorizontalSensitivity"), TEXT("수평 감도"), 0.01f, 10.0f, 1.0f, TEXT("1.00"));
		AddSliderRow(Tree, Controls, TEXT("VerticalSensitivity"), TEXT("수직 감도"), 0.01f, 10.0f, 1.0f, TEXT("1.00"));
		AddSliderRow(Tree, Controls, TEXT("AimSensitivity"), TEXT("ADS 감도 배율"), 0.01f, 2.0f, 1.0f, TEXT("1.00"));
		AddToggleRow(Tree, Controls, TEXT("InvertY"), TEXT("Y축 반전"), false);
		AddSectionTitle(Tree, Controls, TEXT("Gameplay"), TEXT("GAMEPLAY"));
		AddToggleRow(Tree, Controls, TEXT("DynamicCrosshair"), TEXT("동적 크로스헤어"), true);

		UVerticalBox* Audio = MakePage(Tree, Switcher, TEXT("Page_Audio"));
		AddSectionTitle(Tree, Audio, TEXT("Volume"), TEXT("VOLUME"));
		AddSliderRow(Tree, Audio, TEXT("MasterVolume"), TEXT("마스터"), 0.0f, 1.0f, 1.0f, TEXT("100"));
		AddSliderRow(Tree, Audio, TEXT("MusicVolume"), TEXT("음악"), 0.0f, 1.0f, 1.0f, TEXT("100"));
		AddSliderRow(Tree, Audio, TEXT("SfxVolume"), TEXT("효과음"), 0.0f, 1.0f, 1.0f, TEXT("100"));
		AddSliderRow(Tree, Audio, TEXT("UiVolume"), TEXT("UI"), 0.0f, 1.0f, 1.0f, TEXT("100"));
		AddSliderRow(Tree, Audio, TEXT("VoiceVolume"), TEXT("음성"), 0.0f, 1.0f, 1.0f, TEXT("100"));
		AddSectionTitle(Tree, Audio, TEXT("Voice"), TEXT("VOICE"));
		AddToggleRow(Tree, Audio, TEXT("MasterMute"), TEXT("전체 음소거"), false);
		AddToggleRow(Tree, Audio, TEXT("VoiceChat"), TEXT("음성 채팅"), true);

		UVerticalBox* Settings = MakePage(Tree, Switcher, TEXT("Page_Settings"));
		AddSectionTitle(Tree, Settings, TEXT("Options"), TEXT("OPTIONS"));
		AddSelectorRow(Tree, Settings, TEXT("Language"), TEXT("언어"), TEXT("한국어"), 2, 0);

		UVerticalBox* Smart = MakePage(Tree, Switcher, TEXT("Page_SmartTuning"), true);
		AddSectionTitle(Tree, Smart, TEXT("TargetExperience"), TEXT("TARGET EXPERIENCE"));
		AddSelectorRow(Tree, Smart, TEXT("TargetFPS"), TEXT("목표 프레임"), TEXT("120 FPS"), 18, 5);
		UBorder* TargetHelpCard = MakeBorder(Tree, TEXT("Border_TargetFPSHelp"), FLinearColor(0.015f, 0.055f, 0.058f, 0.56f), FMargin(12.0f, 8.0f));
		SetVBoxAuto(Smart->AddChild(TargetHelpCard), FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		UTextBlock* TargetHelp = MakeText(Tree, TEXT("TXT_TargetFPSHelp"), TEXT("목표 프레임은 성능 분석 기준입니다. 화면 설정의 FPS 제한과는 별개입니다."), 11, CSecondaryText, ETextJustify::Left, false);
		TargetHelp->SetAutoWrapText(true);
		TargetHelpCard->SetContent(TargetHelp);
		AddActionRow(Tree, Smart, TEXT("비주얼 우선 · 화질 중심"), TEXT("Btn_Profile_Visual60"), TEXT("프로필 적용"));
		AddActionRow(Tree, Smart, TEXT("균형 · 화질/성능 균형"), TEXT("Btn_Profile_Balanced120"), TEXT("프로필 적용"));
		AddActionRow(Tree, Smart, TEXT("경쟁 · 가시성/프레임 우선"), TEXT("Btn_Profile_Competitive144"), TEXT("프로필 적용"));
		AddActionRow(Tree, Smart, TEXT("최고 화질 · 최대 품질"), TEXT("Btn_Profile_MaxQuality"), TEXT("프로필 적용"));
		AddSpacer(Tree, Smart, 8.0f);
		AddSectionTitle(Tree, Smart, TEXT("SessionAdvisor"), TEXT("SESSION ADVISOR"));

		auto AddAdvisorMetric = [Tree, Smart](const TCHAR* Key, const TCHAR* Label, const TCHAR* Initial)
		{
			UBorder* Row = MakeRowShell(Tree, Smart, *FString::Printf(TEXT("Border_Advisor_%s"), Key));
			UHorizontalBox* H = NewWidget<UHorizontalBox>(Tree, NAME_None);
			Row->SetContent(H);
			SetHBoxFill(H->AddChild(MakeText(Tree, *FString::Printf(TEXT("TXT_Advisor%sLabel"), Key), Label, 13, CSecondaryText, ETextJustify::Left, true)), 1.0f, FMargin(0.0f), VAlign_Center);
			SetHBoxAuto(H->AddChild(MakeText(Tree, *FString::Printf(TEXT("TXT_Advisor%sValue"), Key), Initial, 15, CPrimaryText, ETextJustify::Right, true)), FMargin(14.0f, 0.0f, 0.0f, 0.0f), VAlign_Center);
		};
		AddAdvisorMetric(TEXT("Target"), TEXT("목표 프레임"), TEXT("120 FPS"));
		AddAdvisorMetric(TEXT("Average"), TEXT("지난 세션 평균"), TEXT("--"));
		AddAdvisorMetric(TEXT("OneLow"), TEXT("1% Low"), TEXT("--"));
		AddAdvisorMetric(TEXT("Miss"), TEXT("목표 미달 비율"), TEXT("--"));

		UBorder* AdvisorCard = MakeBorder(Tree, TEXT("Border_AdvisorRecommendation"), FLinearColor(0.020f, 0.110f, 0.115f, 0.62f), FMargin(14.0f, 12.0f));
		SetVBoxAuto(Smart->AddChild(AdvisorCard), FMargin(0.0f, 6.0f, 0.0f, 8.0f));
		UTextBlock* AdvisorText = MakeText(Tree, TEXT("TXT_AdvisorRecommendation"), TEXT("플레이 데이터를 수집한 뒤 실제 세션 기준 추천을 제공합니다."), 13, CSecondaryText, ETextJustify::Left, false);
		AdvisorText->SetAutoWrapText(true);
		AdvisorCard->SetContent(AdvisorText);
		AddActionRow(Tree, Smart, TEXT("세션 추천"), TEXT("Btn_ApplyAdvisorRecommendation"), TEXT("추천 적용"));

		Switcher->SetActiveWidgetIndex(0);

		// Context detail panel
		USizeBox* RightSize = NewWidget<USizeBox>(Tree, TEXT("SB_RightDetail"));
		RightSize->SetWidthOverride(390.0f);
		SetHBoxAuto(Body->AddChild(RightSize));
		UBorder* Right = MakeBorder(Tree, TEXT("Border_RightDetailPanel"), FLinearColor(0.008f, 0.025f, 0.027f, 0.54f), FMargin(10.0f));
		RightSize->SetContent(Right);
		UScrollBox* DetailScroll = NewWidget<UScrollBox>(Tree, TEXT("Scroll_RightDetail"));
		DetailScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
		DetailScroll->SetScrollbarThickness(FVector2D(3.0f, 3.0f));
		DetailScroll->SetAnimateWheelScrolling(true);
		DetailScroll->SetWheelScrollMultiplier(42.0f);
		Right->SetContent(DetailScroll);
		UVerticalBox* Detail = NewWidget<UVerticalBox>(Tree, TEXT("VB_RightDetail"));
		if (UScrollBoxSlot* DetailSlot = Cast<UScrollBoxSlot>(DetailScroll->AddChild(Detail)))
		{
			DetailSlot->SetPadding(FMargin(6.0f));
			DetailSlot->SetHorizontalAlignment(HAlign_Fill);
			DetailSlot->SetVerticalAlignment(VAlign_Top);
		}

		SetVBoxAuto(Detail->AddChild(MakeText(Tree, TEXT("TXT_DetailCategory"), TEXT("GRAPHICS"), 10, CAccent, ETextJustify::Left, true)), FMargin(0.0f, 0.0f, 0.0f, 3.0f));
		SetVBoxAuto(Detail->AddChild(MakeText(Tree, TEXT("TXT_DetailTitle"), TEXT("전체 품질"), 25, CPrimaryText, ETextJustify::Left, true)), FMargin(0.0f, 0.0f, 0.0f, 5.0f));

		UBorder* CurrentBadge = MakeBorder(Tree, TEXT("Border_DetailCurrent"), FLinearColor(0.020f, 0.250f, 0.235f, 0.44f), FMargin(9.0f, 4.0f));
		SetVBoxAuto(Detail->AddChild(CurrentBadge), FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		CurrentBadge->SetContent(MakeText(Tree, TEXT("TXT_DetailCurrentValue"), TEXT("현재 HIGH"), 12, CPrimaryText, ETextJustify::Left, true));

		UTextBlock* DetailBody = MakeText(Tree, TEXT("TXT_DetailBody"), TEXT("여러 Scalability 항목을 하나의 프리셋으로 동시에 조절합니다."), 13, CSecondaryText);
		DetailBody->SetAutoWrapText(true);
		SetVBoxAuto(Detail->AddChild(DetailBody), FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		auto AddExplainCard = [Tree, Detail](const TCHAR* Prefix, const TCHAR* Heading, const TCHAR* Initial)
		{
			UBorder* Card = MakeBorder(Tree, *FString::Printf(TEXT("Border_Explain_%s"), Prefix), FLinearColor(0.015f, 0.055f, 0.058f, 0.60f), FMargin(9.0f, 7.0f));
			SetVBoxAuto(Detail->AddChild(Card), FMargin(0.0f, 0.0f, 0.0f, 5.0f));
			UVerticalBox* V = NewWidget<UVerticalBox>(Tree, NAME_None);
			Card->SetContent(V);
			SetVBoxAuto(V->AddChild(MakeText(Tree, *FString::Printf(TEXT("TXT_Explain%sTitle"), Prefix), Heading, 9, CAccent, ETextJustify::Left, true)), FMargin(0.0f, 0.0f, 0.0f, 2.0f));
			UTextBlock* Text = MakeText(Tree, *FString::Printf(TEXT("TXT_Detail%s"), Prefix), Initial, 11, CSecondaryText, ETextJustify::Left, false);
			Text->SetAutoWrapText(true);
			SetVBoxAuto(V->AddChild(Text));
		};
		AddExplainCard(TEXT("GameplayImpact"), TEXT("게임플레이 영향"), TEXT("플레이 상황에서 체감되는 변화를 설명합니다."));
		AddExplainCard(TEXT("PerformanceCost"), TEXT("성능 비용"), TEXT("GPU / CPU / VRAM 영향"));
		AddExplainCard(TEXT("Recommendation"), TEXT("추천"), TEXT("설정 선택 가이드를 제공합니다."));

		UWidgetSwitcher* DetailVisual = NewWidget<UWidgetSwitcher>(Tree, TEXT("Switcher_DetailVisual"));
		SetVBoxAuto(Detail->AddChild(DetailVisual), FMargin(0.0f, 0.0f, 0.0f, 14.0f));

		// Graphics-only live scene card.
		UBorder* GraphicsVisual = MakeBorder(Tree, TEXT("Panel_GraphicsPreview"), FLinearColor(0.008f, 0.025f, 0.027f, 0.52f), FMargin(10.0f));
		DetailVisual->AddChild(GraphicsVisual);
		UVerticalBox* GraphicsVisualV = NewWidget<UVerticalBox>(Tree, TEXT("VB_GraphicsPreview"));
		GraphicsVisual->SetContent(GraphicsVisualV);
		SetVBoxAuto(GraphicsVisualV->AddChild(MakeText(Tree, TEXT("TXT_PreviewEyebrow"), TEXT("LIVE SCENE"), 10, CAccent, ETextJustify::Left, true)), FMargin(0.0f, 0.0f, 0.0f, 7.0f));

		// A/B Visual Performance Lab. Baseline is a frozen capture; Current stays live.
		UHorizontalBox* LabImages = NewWidget<UHorizontalBox>(Tree, TEXT("HB_LabImages"));
		SetVBoxAuto(GraphicsVisualV->AddChild(LabImages), FMargin(0.0f, 0.0f, 0.0f, 8.0f));

		auto AddLabImage = [Tree, LabImages](const TCHAR* Prefix, const TCHAR* Label, const bool bCurrent)
		{
			UVerticalBox* Column = NewWidget<UVerticalBox>(Tree, *FString::Printf(TEXT("VB_Lab%s"), Prefix));
			SetHBoxFill(LabImages->AddChild(Column), 1.0f, FMargin(bCurrent ? 5.0f : 0.0f, 0.0f, bCurrent ? 0.0f : 5.0f, 0.0f), VAlign_Fill);
			SetVBoxAuto(Column->AddChild(MakeText(Tree, *FString::Printf(TEXT("TXT_Lab%sLabel"), Prefix), Label, 10, CMutedText, ETextJustify::Left, true)), FMargin(0.0f, 0.0f, 0.0f, 4.0f));

			USizeBox* ImageSize = NewWidget<USizeBox>(Tree, *FString::Printf(TEXT("SB_Lab%sPreview"), Prefix));
			ImageSize->SetHeightOverride(116.0f);
			SetVBoxAuto(Column->AddChild(ImageSize));
			UBorder* Frame = MakeBorder(Tree, *FString::Printf(TEXT("Border_Lab%sFrame"), Prefix), FLinearColor(0.030f, 0.300f, 0.285f, 0.32f), FMargin(4.0f));
			ImageSize->SetContent(Frame);
			UBorder* Inner = MakeBorder(Tree, *FString::Printf(TEXT("Border_Lab%sInner"), Prefix), FLinearColor(0.006f, 0.010f, 0.012f, 1.0f));
			Frame->SetContent(Inner);
			UOverlay* Overlay = NewWidget<UOverlay>(Tree, *FString::Printf(TEXT("OV_Lab%s"), Prefix));
			Inner->SetContent(Overlay);

			const FName ImageName = bCurrent ? TEXT("IMG_Preview") : TEXT("IMG_BaselinePreview");
			UImage* Image = NewWidget<UImage>(Tree, ImageName);
			Image->SetVisibility(ESlateVisibility::Collapsed);
			SetOverlayFill(Overlay->AddChild(Image));

			const FName StatusName = bCurrent ? TEXT("TXT_PreviewStatus") : TEXT("TXT_BaselineStatus");
			UTextBlock* Status = MakeText(
				Tree,
				StatusName,
				bCurrent ? TEXT("OptionsPreview 카메라 연결 대기 중") : TEXT("기준 화면을 캡처하세요"),
				10,
				CSecondaryText,
				ETextJustify::Center,
				true);
			Status->SetAutoWrapText(true);
			SetOverlayCenter(Overlay->AddChild(Status));

			SetVBoxAuto(Column->AddChild(MakeText(Tree, *FString::Printf(TEXT("TXT_Lab%sValue"), Prefix), bCurrent ? TEXT("현재 설정") : TEXT("기준 미캡처"), 11, CPrimaryText, ETextJustify::Left, true)), FMargin(0.0f, 5.0f, 0.0f, 0.0f));
		};

		AddLabImage(TEXT("Baseline"), TEXT("기준 화면"), false);
		AddLabImage(TEXT("Current"), TEXT("현재 화면"), true);

		UHorizontalBox* LabMetrics = NewWidget<UHorizontalBox>(Tree, TEXT("HB_LabMetrics"));
		SetVBoxAuto(GraphicsVisualV->AddChild(LabMetrics), FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		auto AddLabMetric = [Tree, LabMetrics](const TCHAR* Key, const TCHAR* Label, const TCHAR* Initial)
		{
			UBorder* Metric = MakeBorder(Tree, *FString::Printf(TEXT("Border_LabMetric_%s"), Key), FLinearColor(0.015f, 0.050f, 0.053f, 0.70f), FMargin(7.0f, 5.0f));
			SetHBoxFill(LabMetrics->AddChild(Metric), 1.0f, FMargin(2.0f), VAlign_Fill);
			UVerticalBox* V = NewWidget<UVerticalBox>(Tree, NAME_None);
			Metric->SetContent(V);
			SetVBoxAuto(V->AddChild(MakeText(Tree, NAME_None, Label, 9, CMutedText, ETextJustify::Left, true)));
			SetVBoxAuto(V->AddChild(MakeText(Tree, *FString::Printf(TEXT("TXT_Lab%s"), Key), Initial, 11, CPrimaryText, ETextJustify::Left, true)));
		};
		AddLabMetric(TEXT("BaselineFrameTime"), TEXT("BASELINE"), TEXT("--"));
		AddLabMetric(TEXT("CurrentFrameTime"), TEXT("CURRENT"), TEXT("--"));
		AddLabMetric(TEXT("Delta"), TEXT("DELTA"), TEXT("--"));

		USizeBox* CaptureButtonSize = NewWidget<USizeBox>(Tree, TEXT("SB_LabCaptureBaseline"));
		CaptureButtonSize->SetHeightOverride(32.0f);
		SetVBoxAuto(GraphicsVisualV->AddChild(CaptureButtonSize), FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		CaptureButtonSize->SetContent(MakeButton(Tree, TEXT("Btn_LabCaptureBaseline"), TEXT("기준 캡처"), 11, CAccentSoft, CPrimaryText));

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



	static bool ValidateGeneratedWidgetTree(UWidgetBlueprint* Blueprint, FString& OutError)
	{
		if (!Blueprint || !Blueprint->WidgetTree || !Blueprint->WidgetTree->RootWidget)
		{
			OutError = TEXT("WidgetTree or RootWidget is missing.");
			return false;
		}

		TSet<FName> WidgetNames;
		bool bValid = true;
		Blueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (!Widget || !bValid)
			{
				return;
			}

			const FName WidgetName = Widget->GetFName();
			if (WidgetName.IsNone())
			{
				return;
			}

			if (WidgetNames.Contains(WidgetName))
			{
				OutError = FString::Printf(TEXT("Duplicate widget name: %s"), *WidgetName.ToString());
				bValid = false;
				return;
			}
			WidgetNames.Add(WidgetName);
		});

		return bValid;
	}

	static bool FinalizeGeneratedBlueprint(float, TWeakObjectPtr<UWidgetBlueprint> WeakBlueprint)
	{
		UWidgetBlueprint* Blueprint = WeakBlueprint.Get();
		if (!Blueprint)
		{
			return false;
		}

		FString ValidationError;
		if (!ValidateGeneratedWidgetTree(Blueprint, ValidationError))
		{
			Blueprint->RemoveFromRoot();
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
				FString::Printf(TEXT("WBP validation failed. / WBP 생성 검증 실패\n\n%s"), *ValidationError)));
			return false;
		}

		// Compile after the asset-creation command has returned.  UMG compilation performs
		// skeleton/reinstancing work; deferring it one editor tick avoids compiling while
		// AssetTools/WidgetBlueprintFactory are still on the creation stack.
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (Blueprint->Status == BS_Error)
		{
			Blueprint->RemoveFromRoot();
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
				TEXT("WBP compilation failed; the asset was not saved. Check Message Log, fix the errors, and compile again. / WBP 컴파일 실패: Message Log를 확인하세요.")));
			return false;
		}

		UPackage* Package = Blueprint->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(
			Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		const bool bSaved = UPackage::SavePackage(Package, Blueprint, *Filename, SaveArgs);
		Blueprint->RemoveFromRoot();

		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
			bSaved
				? FString::Printf(TEXT("Created and saved / 생성 및 저장 완료: %s"), *Package->GetName())
				: FString(TEXT("WBP compiled, but package saving failed. Check Output Log and write permissions. / 패키지 저장 실패"))));
		return false;
	}

	static void GenerateAtPath(const FString& PackagePath)
	{
		const FString AssetName = TEXT("WBP_OptionsScreen_Auto");
		const FString LongPackageName = PackagePath + TEXT("/") + AssetName;
		const FString ObjectPath = LongPackageName + TEXT(".") + AssetName;

		if (FindObject<UObject>(nullptr, *ObjectPath) || FPackageName::DoesPackageExist(LongPackageName))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
				FString::Printf(TEXT("An asset already exists at %s. Preserve your edits before renaming or removing it, then fix redirectors before regenerating. / 기존 에셋을 보존한 뒤 이름과 리디렉터를 정리하세요."), *LongPackageName)));
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
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Widget Blueprint creation failed. / Widget Blueprint 생성 실패")));
			return;
		}

		if (Blueprint->WidgetTree->RootWidget)
		{
			Blueprint->WidgetTree->RemoveWidget(Blueprint->WidgetTree->RootWidget);
			Blueprint->WidgetTree->RootWidget = nullptr;
		}

		BuildWidgetTree(Blueprint);
		if (!Blueprint->WidgetTree->RootWidget)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Widget tree creation failed. / Widget Tree 생성 실패")));
			return;
		}

		FString ValidationError;
		if (!ValidateGeneratedWidgetTree(Blueprint, ValidationError))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
				FString::Printf(TEXT("WBP validation failed. / WBP 생성 검증 실패\n\n%s"), *ValidationError)));
			return;
		}

		// Hold the new asset until the deferred editor-tick compile/save completes.
		Blueprint->AddToRoot();
		const TWeakObjectPtr<UWidgetBlueprint> WeakBlueprint(Blueprint);
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WeakBlueprint](float DeltaTime)
			{
				return FinalizeGeneratedBlueprint(DeltaTime, WeakBlueprint);
			}),
			0.10f);
	}

	static void Generate()
	{
		GenerateAtPath(TEXT("/Game/UI/Settings"));
	}

	static FAutoConsoleCommand GenerateCommand(
		TEXT("FPS.UI.GenerateOptionsWBP"),
		TEXT("Creates /Game/UI/Settings/WBP_OptionsScreen_Auto using ShooterOptionsScreen as the parent class. Does not overwrite an existing asset."),
		FConsoleCommandDelegate::CreateStatic(&Generate));
}

#endif
