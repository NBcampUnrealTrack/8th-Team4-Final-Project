#if WITH_EDITOR

#include "UI/InGame/Menu/ShooterInGameMenuScreen.h"

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
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

namespace ShooterInGameMenuWbpGenerator
{
	// Same visual language as WBP_OptionsScreen_Auto v10:
	// neutral dark glass surfaces, cyan only for focus/accent, no default white-outline buttons.
	static const FLinearColor CMain(0.004f, 0.015f, 0.017f, 0.94f);
	static const FLinearColor CPanel(0.010f, 0.030f, 0.032f, 0.78f);
	static const FLinearColor CButton(0.028f, 0.060f, 0.064f, 0.78f);
	static const FLinearColor CButtonHover(0.030f, 0.210f, 0.200f, 0.72f);
	static const FLinearColor CButtonPressed(0.024f, 0.300f, 0.285f, 0.88f);
	static const FLinearColor CAccent(0.030f, 0.900f, 0.820f, 1.00f);
	static const FLinearColor CPrimaryText(0.970f, 0.990f, 0.990f, 1.00f);
	static const FLinearColor CSecondaryText(0.690f, 0.770f, 0.780f, 1.00f);
	static const FLinearColor CMutedText(0.430f, 0.550f, 0.560f, 1.00f);

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
		const FString& Value,
		const int32 FontSize,
		const FLinearColor& Color,
		const ETextJustify::Type Justify = ETextJustify::Left,
		const bool bBold = false,
		const int32 LetterSpacing = 0)
	{
		UTextBlock* Text = NewWidget<UTextBlock>(Tree, Name);
		Text->SetText(FText::FromString(Value));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(Justify);

		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Font.LetterSpacing = LetterSpacing;
		if (bBold)
		{
			Font.TypefaceFontName = TEXT("Bold");
		}
		Text->SetFont(Font);
		return Text;
	}

	static UBorder* MakeBorder(
		UWidgetTree* Tree,
		const FName Name,
		const FLinearColor& Color,
		const FMargin Padding = FMargin(0.0f))
	{
		UBorder* Border = NewWidget<UBorder>(Tree, Name);
		Border->SetBrushColor(Color);
		Border->SetPadding(Padding);
		return Border;
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

	static void SetHBoxAuto(UPanelSlot* BaseSlot, const FMargin Padding = FMargin(0.0f), const EVerticalAlignment VAlign = VAlign_Center)
	{
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(BaseSlot))
		{
			Slot->SetSize(AutoSize());
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign);
		}
	}

	static void SetHBoxFill(UPanelSlot* BaseSlot, const float Weight = 1.0f, const FMargin Padding = FMargin(0.0f), const EVerticalAlignment VAlign = VAlign_Center)
	{
		if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(BaseSlot))
		{
			Slot->SetSize(FillSize(Weight));
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign);
		}
	}

	static void ApplyButtonStyle(
		UButton* Button,
		const FLinearColor& Normal,
		const FLinearColor& Hovered,
		const FLinearColor& Pressed)
	{
		FButtonStyle Style;
		Style.SetNormal(FSlateColorBrush(Normal));
		Style.SetHovered(FSlateColorBrush(Hovered));
		Style.SetPressed(FSlateColorBrush(Pressed));
		Style.SetDisabled(FSlateColorBrush(FLinearColor(Normal.R, Normal.G, Normal.B, Normal.A * 0.28f)));
		Style.SetNormalPadding(FMargin(0.0f));
		Style.SetPressedPadding(FMargin(0.0f));
		Button->SetStyle(Style);
		Button->SetBackgroundColor(FLinearColor::White);
		Button->SetColorAndOpacity(FLinearColor::White);
	}

	static void AddMenuButton(
		UWidgetTree* Tree,
		UVerticalBox* Parent,
		const FName ButtonName,
		const FString& Label,
		const FString& Meta,
		const FString& Index)
	{
		USizeBox* RowSize = NewWidget<USizeBox>(Tree, *FString::Printf(TEXT("SB_%s"), *ButtonName.ToString()));
		RowSize->SetHeightOverride(62.0f);
		SetVBoxAuto(Parent->AddChild(RowSize), FMargin(0.0f, 0.0f, 0.0f, 8.0f));

		UOverlay* RowOverlay = NewWidget<UOverlay>(Tree, *FString::Printf(TEXT("OV_%s"), *ButtonName.ToString()));
		RowSize->SetContent(RowOverlay);

		UButton* Button = NewWidget<UButton>(Tree, ButtonName);
		ApplyButtonStyle(
			Button,
			CButton,
			CButtonHover,
			CButtonPressed);
		SetOverlayFill(RowOverlay->AddChild(Button));

		UHorizontalBox* Content = NewWidget<UHorizontalBox>(Tree, *FString::Printf(TEXT("HB_%s_Content"), *ButtonName.ToString()));
		Button->SetContent(Content);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Content->Slot))
		{
			Slot->SetPadding(FMargin(16.0f, 0.0f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		USizeBox* IndexSize = NewWidget<USizeBox>(Tree, NAME_None);
		IndexSize->SetWidthOverride(38.0f);
		SetHBoxAuto(Content->AddChild(IndexSize), FMargin(0.0f, 0.0f, 12.0f, 0.0f));
		IndexSize->SetContent(MakeText(
			Tree,
			*FString::Printf(TEXT("TXT_%s_Index"), *ButtonName.ToString()),
			Index,
			10,
			CAccent,
			ETextJustify::Center,
			true,
			80));

		UTextBlock* LabelText = MakeText(
			Tree,
			*FString::Printf(TEXT("TXT_%s_Label"), *ButtonName.ToString()),
			Label,
			17,
			CPrimaryText,
			ETextJustify::Left,
			true);
		SetHBoxFill(Content->AddChild(LabelText), 1.0f);

		UTextBlock* MetaText = MakeText(
			Tree,
			*FString::Printf(TEXT("TXT_%s_Meta"), *ButtonName.ToString()),
			Meta,
			9,
			CMutedText,
			ETextJustify::Right,
			true,
			100);
		SetHBoxAuto(Content->AddChild(MetaText), FMargin(12.0f, 0.0f, 14.0f, 0.0f));

		UTextBlock* Arrow = MakeText(
			Tree,
			*FString::Printf(TEXT("TXT_%s_Arrow"), *ButtonName.ToString()),
			TEXT(">"),
			16,
			CAccent,
			ETextJustify::Center,
			true);
		SetHBoxAuto(Content->AddChild(Arrow));

		// Thin accent rail. Every action uses the same neutral resting state.
		USizeBox* RailSize = NewWidget<USizeBox>(Tree, *FString::Printf(TEXT("SB_%s_Rail"), *ButtonName.ToString()));
		RailSize->SetWidthOverride(3.0f);
		SetOverlayFill(RowOverlay->AddChild(RailSize));
		if (UOverlaySlot* RailSlot = Cast<UOverlaySlot>(RailSize->Slot))
		{
			RailSlot->SetHorizontalAlignment(HAlign_Left);
			RailSlot->SetVerticalAlignment(VAlign_Fill);
		}
		RailSize->SetContent(MakeBorder(
			Tree,
			*FString::Printf(TEXT("Border_%s_Rail"), *ButtonName.ToString()),
			FLinearColor(CAccent.R, CAccent.G, CAccent.B, 0.26f)));
	}

	static void BuildWidgetTree(UWidgetBlueprint* Blueprint)
	{
		UWidgetTree* Tree = Blueprint->WidgetTree;
		check(Tree);

		UOverlay* Root = NewWidget<UOverlay>(Tree, TEXT("Overlay_Root"));
		Tree->RootWidget = Root;

		UBackgroundBlur* Blur = NewWidget<UBackgroundBlur>(Tree, TEXT("BackgroundBlur_BG"));
		Blur->SetBlurStrength(16.0f);
		Blur->SetApplyAlphaToBlur(true);
		SetOverlayFill(Root->AddChild(Blur));

		UBorder* Dim = MakeBorder(Tree, TEXT("Border_Dim"), FLinearColor(0.0f, 0.0f, 0.0f, 0.34f));
		SetOverlayFill(Root->AddChild(Dim));

		USizeBox* PanelSize = NewWidget<USizeBox>(Tree, TEXT("SB_MenuPanel"));
		PanelSize->SetWidthOverride(620.0f);
		PanelSize->SetHeightOverride(670.0f);
		SetOverlayCenter(Root->AddChild(PanelSize));

		UBorder* MainPanel = MakeBorder(Tree, TEXT("Border_MainGlassPanel"), CMain, FMargin(28.0f, 24.0f));
		PanelSize->SetContent(MainPanel);

		UVerticalBox* Main = NewWidget<UVerticalBox>(Tree, TEXT("VB_Main"));
		MainPanel->SetContent(Main);

		// Header
		UHorizontalBox* HeaderTop = NewWidget<UHorizontalBox>(Tree, TEXT("HB_HeaderTop"));
		SetVBoxAuto(Main->AddChild(HeaderTop));
		SetHBoxFill(HeaderTop->AddChild(MakeText(Tree, TEXT("TXT_Eyebrow"), TEXT("SYSTEM MENU"), 10, CAccent, ETextJustify::Left, true, 140)), 1.0f);

		UBorder* PauseBadge = MakeBorder(Tree, TEXT("Border_PauseBadge"), FLinearColor(0.030f, 0.180f, 0.175f, 0.42f), FMargin(9.0f, 4.0f));
		SetHBoxAuto(HeaderTop->AddChild(PauseBadge));
		PauseBadge->SetContent(MakeText(Tree, TEXT("TXT_PauseBadge"), TEXT("PAUSED"), 9, CSecondaryText, ETextJustify::Center, true, 100));

		SetVBoxAuto(Main->AddChild(MakeText(Tree, TEXT("TXT_Title"), TEXT("일시 정지"), 34, CPrimaryText, ETextJustify::Left, true)), FMargin(0.0f, 4.0f, 0.0f, 4.0f));
		UTextBlock* Subtitle = MakeText(
			Tree,
			TEXT("TXT_Subtitle"),
			TEXT("게임을 계속하거나 세션 작업을 선택하세요."),
			13,
			CSecondaryText);
		SetVBoxAuto(Main->AddChild(Subtitle), FMargin(0.0f, 0.0f, 0.0f, 18.0f));

		UHorizontalBox* Divider = NewWidget<UHorizontalBox>(Tree, TEXT("HB_Divider"));
		SetVBoxAuto(Main->AddChild(Divider), FMargin(0.0f, 0.0f, 0.0f, 22.0f));
		USizeBox* AccentLine = NewWidget<USizeBox>(Tree, NAME_None);
		AccentLine->SetWidthOverride(52.0f);
		AccentLine->SetHeightOverride(2.0f);
		SetHBoxAuto(Divider->AddChild(AccentLine), FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		AccentLine->SetContent(MakeBorder(Tree, TEXT("Border_HeaderAccent"), CAccent));
		USizeBox* DividerLine = NewWidget<USizeBox>(Tree, NAME_None);
		DividerLine->SetHeightOverride(1.0f);
		SetHBoxFill(Divider->AddChild(DividerLine), 1.0f);
		DividerLine->SetContent(MakeBorder(Tree, TEXT("Border_HeaderDivider"), FLinearColor(1.0f, 1.0f, 1.0f, 0.08f)));

		// Actions
		UVerticalBox* Actions = NewWidget<UVerticalBox>(Tree, TEXT("VB_Actions"));
		SetVBoxAuto(Main->AddChild(Actions));
		AddMenuButton(Tree, Actions, TEXT("Btn_Resume"), TEXT("게임 계속"), TEXT("RESUME"), TEXT("01"));
		AddMenuButton(Tree, Actions, TEXT("Btn_Options"), TEXT("옵션"), TEXT("OPTIONS"), TEXT("02"));
		AddMenuButton(Tree, Actions, TEXT("Btn_Surrender"), TEXT("항복"), TEXT("SURRENDER"), TEXT("03"));
		AddMenuButton(Tree, Actions, TEXT("Btn_ReturnToLobby"), TEXT("로비로 돌아가기"), TEXT("LOBBY"), TEXT("04"));
		AddMenuButton(Tree, Actions, TEXT("Btn_QuitGame"), TEXT("게임 종료"), TEXT("QUIT"), TEXT("05"));

		// Status area is bound by the existing gameplay code (surrender cooldown/status etc.).
		USizeBox* StatusSize = NewWidget<USizeBox>(Tree, TEXT("SB_Status"));
		StatusSize->SetMinDesiredHeight(52.0f);
		SetVBoxAuto(Main->AddChild(StatusSize), FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		UBorder* StatusPanel = MakeBorder(Tree, TEXT("Border_Status"), CPanel, FMargin(12.0f, 8.0f));
		StatusSize->SetContent(StatusPanel);
		UTextBlock* Status = MakeText(Tree, TEXT("Text_Status"), TEXT(""), 11, CSecondaryText, ETextJustify::Center);
		Status->SetAutoWrapText(true);
		StatusPanel->SetContent(Status);

		// Flexible lower breathing room keeps the card balanced at different DPI scales.
		USpacer* BottomSpace = NewWidget<USpacer>(Tree, NAME_None);
		SetVBoxFill(Main->AddChild(BottomSpace), 1.0f);
	}

	static void Generate()
	{
		const FString PackagePath = TEXT("/Game/UI/InGame/Menu");
		const FString AssetName = TEXT("WBP_InGameMenu_Auto");
		const FString LongPackageName = PackagePath + TEXT("/") + AssetName;
		const FString ObjectPath = LongPackageName + TEXT(".") + AssetName;

		if (FindObject<UObject>(nullptr, *ObjectPath) || FPackageName::DoesPackageExist(LongPackageName))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
				TEXT("WBP_InGameMenu_Auto가 이미 존재합니다. 기존 에셋을 지우거나 이름을 바꾼 뒤 다시 실행하세요.")));
			return;
		}

		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = UShooterInGameMenuScreen::StaticClass();
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
			TEXT("생성 완료: /Game/UI/InGame/Menu/WBP_InGameMenu_Auto\n\nPlayerController의 UI Init Component -> In Game Menu Class에 이 WBP를 지정하세요.")));
	}

	static FAutoConsoleCommand GenerateCommand(
		TEXT("FPS.UI.GenerateInGameMenuWBP"),
		TEXT("Creates /Game/UI/InGame/Menu/WBP_InGameMenu_Auto using ShooterInGameMenuScreen as the parent class."),
		FConsoleCommandDelegate::CreateStatic(&Generate));
}

#endif
