#if WITH_EDITOR

#include "UI/InGame/Menu/ShooterSurrenderVoteScreen.h"
#include "UI/InGame/Menu/ShooterSurrenderVoterRow.h"

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
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

namespace ShooterSurrenderVoteWbpGenerator
{
	static const FLinearColor CDim(0.00f, 0.01f, 0.02f, 0.52f);
	static const FLinearColor CPanel(0.02f, 0.07f, 0.08f, 0.88f);
	static const FLinearColor CPanelSoft(0.03f, 0.10f, 0.11f, 0.72f);
	static const FLinearColor CCard(0.04f, 0.13f, 0.14f, 0.72f);
	static const FLinearColor CAccent(0.05f, 0.93f, 0.87f, 1.00f);
	static const FLinearColor CPrimary(0.05f, 0.42f, 0.40f, 0.95f);
	static const FLinearColor CPrimaryHover(0.08f, 0.58f, 0.54f, 1.00f);
	static const FLinearColor CPrimaryPressed(0.08f, 0.72f, 0.66f, 1.00f);
	static const FLinearColor CSecondary(0.07f, 0.15f, 0.17f, 0.90f);
	static const FLinearColor CSecondaryHover(0.10f, 0.22f, 0.25f, 0.95f);
	static const FLinearColor CSecondaryPressed(0.12f, 0.27f, 0.30f, 0.98f);
	static const FLinearColor CPrimaryText(0.96f, 0.98f, 0.99f, 1.00f);
	static const FLinearColor CSecondaryText(0.74f, 0.81f, 0.82f, 1.00f);
	static const FLinearColor CMutedText(0.55f, 0.66f, 0.68f, 1.00f);

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
		Text->SetAutoWrapText(true);

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
			Slot->SetVerticalAlignment(VAlign_Center);
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

	static void ApplyButtonStyle(UButton* Button, const FLinearColor& Normal, const FLinearColor& Hovered, const FLinearColor& Pressed)
	{
		FButtonStyle Style;
		Style.SetNormal(FSlateColorBrush(Normal));
		Style.SetHovered(FSlateColorBrush(Hovered));
		Style.SetPressed(FSlateColorBrush(Pressed));
		Style.SetDisabled(FSlateColorBrush(FLinearColor(Normal.R, Normal.G, Normal.B, Normal.A * 0.25f)));
		Style.SetNormalPadding(FMargin(0.0f));
		Style.SetPressedPadding(FMargin(0.0f));
		Button->SetStyle(Style);
		Button->SetBackgroundColor(FLinearColor::White);
		Button->SetColorAndOpacity(FLinearColor::White);
	}

	static void ApplyCheckboxStyle(UCheckBox* CheckBox)
	{
		FCheckBoxStyle Style = CheckBox->GetWidgetStyle();
		Style.UncheckedImage = FSlateColorBrush(FLinearColor(0.06f, 0.15f, 0.17f, 1.0f));
		Style.UncheckedHoveredImage = FSlateColorBrush(FLinearColor(0.06f, 0.15f, 0.17f, 1.0f));
		Style.UncheckedPressedImage = FSlateColorBrush(FLinearColor(0.06f, 0.15f, 0.17f, 1.0f));
		Style.CheckedImage = FSlateColorBrush(CAccent);
		Style.CheckedHoveredImage = FSlateColorBrush(CAccent);
		Style.CheckedPressedImage = FSlateColorBrush(CAccent);
		Style.UndeterminedImage = FSlateColorBrush(FLinearColor(0.20f, 0.26f, 0.27f, 1.0f));
		Style.UndeterminedHoveredImage = FSlateColorBrush(FLinearColor(0.20f, 0.26f, 0.27f, 1.0f));
		Style.UndeterminedPressedImage = FSlateColorBrush(FLinearColor(0.20f, 0.26f, 0.27f, 1.0f));
		CheckBox->SetWidgetStyle(Style);
		// UCheckBox itself has no padding API in UE5.7.
		// Geometry/margins are owned by the parent SizeBox/HorizontalBox slot.
		CheckBox->SetIsEnabled(false);
	}

	static void BuildVoterRowTree(UWidgetBlueprint* Blueprint)
	{
		UWidgetTree* Tree = Blueprint->WidgetTree;

		UBorder* Root = MakeBorder(Tree, TEXT("Border_RowRoot"), CPanelSoft, FMargin(10.0f, 8.0f));
		Tree->RootWidget = Root;

		UHorizontalBox* Row = NewWidget<UHorizontalBox>(Tree, TEXT("HB_Row"));
		Root->SetContent(Row);

		UTextBlock* PlayerName = MakeText(Tree, TEXT("Text_PlayerName"), TEXT("Player 01"), 14, CPrimaryText, ETextJustify::Left, true, 20);
		SetHBoxFill(Row->AddChild(PlayerName), 1.0f, FMargin(0.0f, 0.0f, 12.0f, 0.0f));

		UBorder* StatusBadge = MakeBorder(Tree, TEXT("Border_StatusBadge"), CCard, FMargin(12.0f, 6.0f));
		SetHBoxAuto(Row->AddChild(StatusBadge), FMargin(0.0f, 0.0f, 12.0f, 0.0f));
		UTextBlock* VoteStatus = MakeText(Tree, TEXT("Text_VoteStatus"), TEXT("대기"), 12, CSecondaryText, ETextJustify::Center, true, 12);
		StatusBadge->SetContent(VoteStatus);

		USizeBox* CheckSize = NewWidget<USizeBox>(Tree, TEXT("SB_Check"));
		CheckSize->SetWidthOverride(18.0f);
		CheckSize->SetHeightOverride(18.0f);
		SetHBoxAuto(Row->AddChild(CheckSize));
		UCheckBox* VoteCheck = NewWidget<UCheckBox>(Tree, TEXT("Check_VoteStatus"));
		ApplyCheckboxStyle(VoteCheck);
		CheckSize->SetContent(VoteCheck);
	}

	static void BuildMainTree(UWidgetBlueprint* Blueprint)
	{
		UWidgetTree* Tree = Blueprint->WidgetTree;

		UOverlay* Root = NewWidget<UOverlay>(Tree, TEXT("Overlay_Root"));
		Tree->RootWidget = Root;

		UBackgroundBlur* Blur = NewWidget<UBackgroundBlur>(Tree, TEXT("BackgroundBlur"));
		Blur->SetBlurStrength(10.0f);
		Blur->SetApplyAlphaToBlur(true);
		SetOverlayFill(Root->AddChild(Blur));

		UBorder* Dim = MakeBorder(Tree, TEXT("Border_Dim"), CDim);
		SetOverlayFill(Root->AddChild(Dim));

		USizeBox* CardSize = NewWidget<USizeBox>(Tree, TEXT("SB_Card"));
		CardSize->SetWidthOverride(720.0f);
		CardSize->SetHeightOverride(500.0f);
		SetOverlayCenter(Root->AddChild(CardSize));

		UBorder* Card = MakeBorder(Tree, TEXT("Border_Card"), CPanel, FMargin(24.0f, 20.0f));
		CardSize->SetContent(Card);

		UVerticalBox* Main = NewWidget<UVerticalBox>(Tree, TEXT("VB_Main"));
		Card->SetContent(Main);

		UBorder* AccentLine = MakeBorder(Tree, TEXT("Border_AccentLine"), CAccent);
		AccentLine->SetDesiredSizeScale(FVector2D(1.0f, 1.0f));
		USizeBox* AccentSize = NewWidget<USizeBox>(Tree, TEXT("SB_AccentLine"));
		AccentSize->SetHeightOverride(2.0f);
		AccentSize->SetContent(AccentLine);
		SetVBoxAuto(Main->AddChild(AccentSize), FMargin(0.0f, 0.0f, 0.0f, 14.0f));

		UTextBlock* Meta = MakeText(Tree, TEXT("Text_Meta"), TEXT("TEAM VOTE"), 12, CAccent, ETextJustify::Left, true, 36);
		SetVBoxAuto(Main->AddChild(Meta), FMargin(0.0f, 0.0f, 0.0f, 6.0f));

		UTextBlock* Title = MakeText(Tree, TEXT("Text_Title"), TEXT("항복 투표가 시작되었습니다"), 30, CPrimaryText, ETextJustify::Left, true, 12);
		SetVBoxAuto(Main->AddChild(Title), FMargin(0.0f, 0.0f, 0.0f, 18.0f));

		UHorizontalBox* SummaryRow = NewWidget<UHorizontalBox>(Tree, TEXT("HB_Summary"));
		SetVBoxAuto(Main->AddChild(SummaryRow), FMargin(0.0f, 0.0f, 0.0f, 16.0f));

		UBorder* ApprovalCard = MakeBorder(Tree, TEXT("Border_ApprovalCard"), CCard, FMargin(14.0f, 12.0f));
		SetHBoxFill(SummaryRow->AddChild(ApprovalCard), 1.0f, FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		UVerticalBox* ApprovalBox = NewWidget<UVerticalBox>(Tree, TEXT("VB_ApprovalCard"));
		ApprovalCard->SetContent(ApprovalBox);
		SetVBoxAuto(ApprovalBox->AddChild(MakeText(Tree, TEXT("Text_ApprovalLabel"), TEXT("APPROVAL"), 11, CMutedText, ETextJustify::Left, true, 24)), FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		SetVBoxAuto(ApprovalBox->AddChild(MakeText(Tree, TEXT("Text_ApprovalSummary"), TEXT("찬성 0 / 필요 0"), 18, CPrimaryText, ETextJustify::Left, true, 10)));

		UBorder* RemainingCard = MakeBorder(Tree, TEXT("Border_RemainingCard"), CCard, FMargin(14.0f, 12.0f));
		SetHBoxFill(SummaryRow->AddChild(RemainingCard), 1.0f, FMargin(8.0f, 0.0f, 0.0f, 0.0f));
		UVerticalBox* RemainingBox = NewWidget<UVerticalBox>(Tree, TEXT("VB_RemainingCard"));
		RemainingCard->SetContent(RemainingBox);
		SetVBoxAuto(RemainingBox->AddChild(MakeText(Tree, TEXT("Text_RemainingLabel"), TEXT("TIME"), 11, CMutedText, ETextJustify::Left, true, 24)), FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		SetVBoxAuto(RemainingBox->AddChild(MakeText(Tree, TEXT("Text_RemainingTime"), TEXT("남은 시간 15초"), 18, CPrimaryText, ETextJustify::Left, true, 10)));

		UBorder* ListCard = MakeBorder(Tree, TEXT("Border_ListCard"), CPanelSoft, FMargin(16.0f, 14.0f));
		SetVBoxFill(Main->AddChild(ListCard), 1.0f, FMargin(0.0f, 0.0f, 0.0f, 18.0f));
		UVerticalBox* ListBox = NewWidget<UVerticalBox>(Tree, TEXT("VB_ListCard"));
		ListCard->SetContent(ListBox);
		SetVBoxAuto(ListBox->AddChild(MakeText(Tree, TEXT("Text_ListTitle"), TEXT("투표 현황"), 14, CPrimaryText, ETextJustify::Left, true, 16)), FMargin(0.0f, 0.0f, 0.0f, 10.0f));

		UScrollBox* VoterScroll = NewWidget<UScrollBox>(Tree, TEXT("Scroll_Voters"));
		VoterScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		SetVBoxFill(ListBox->AddChild(VoterScroll), 1.0f);

		UVerticalBox* VoterPanel = NewWidget<UVerticalBox>(Tree, TEXT("Panel_Voters"));
		VoterScroll->AddChild(VoterPanel);

		for (int32 DummyIndex = 0; DummyIndex < 4; ++DummyIndex)
		{
			UBorder* RowDummy = MakeBorder(Tree, *FString::Printf(TEXT("Border_DummyRow_%d"), DummyIndex), FLinearColor(0.008f, 0.026f, 0.030f, 0.98f), FMargin(12.0f, 9.0f));
			SetVBoxAuto(VoterPanel->AddChild(RowDummy), FMargin(0.0f, 0.0f, 0.0f, 8.0f));

			UHorizontalBox* DummyRow = NewWidget<UHorizontalBox>(Tree, *FString::Printf(TEXT("HB_DummyRow_%d"), DummyIndex));
			RowDummy->SetContent(DummyRow);

			UTextBlock* DummyName = MakeText(Tree, *FString::Printf(TEXT("Text_DummyName_%d"), DummyIndex), DummyIndex == 0 ? TEXT("Player 01") : (DummyIndex == 1 ? TEXT("Player 02") : (DummyIndex == 2 ? TEXT("Player 03") : TEXT("Player 04"))), 14, CPrimaryText, ETextJustify::Left, true, 20);
			SetHBoxFill(DummyRow->AddChild(DummyName), 1.0f, FMargin(0.0f, 0.0f, 12.0f, 0.0f));

			UBorder* DummyBadge = MakeBorder(Tree, *FString::Printf(TEXT("Border_DummyBadge_%d"), DummyIndex), FLinearColor(0.020f, 0.070f, 0.075f, 0.98f), FMargin(14.0f, 6.0f));
			SetHBoxAuto(DummyRow->AddChild(DummyBadge), FMargin(0.0f, 0.0f, 12.0f, 0.0f));
			const FString DummyStatus = DummyIndex == 0 ? TEXT("찬성") : (DummyIndex == 1 ? TEXT("대기") : (DummyIndex == 2 ? TEXT("반대") : TEXT("대기")));
			const FLinearColor DummyColor = DummyIndex == 0 ? FLinearColor(0.12f, 1.00f, 0.74f, 1.0f) : (DummyIndex == 2 ? FLinearColor(1.00f, 0.38f, 0.38f, 1.0f) : FLinearColor(1.00f, 0.78f, 0.28f, 1.0f));
			DummyBadge->SetContent(MakeText(Tree, *FString::Printf(TEXT("Text_DummyStatus_%d"), DummyIndex), DummyStatus, 12, DummyColor, ETextJustify::Center, true, 12));
		}

		UHorizontalBox* ActionRow = NewWidget<UHorizontalBox>(Tree, TEXT("HB_ActionRow"));
		SetVBoxAuto(Main->AddChild(ActionRow));

		UButton* ApproveButton = NewWidget<UButton>(Tree, TEXT("Btn_Approve"));
		ApplyButtonStyle(ApproveButton, CSecondary, CPrimaryHover, CPrimaryPressed);
		SetHBoxFill(ActionRow->AddChild(ApproveButton), 1.0f, FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		USizeBox* ApproveSize = NewWidget<USizeBox>(Tree, TEXT("SB_Approve"));
		ApproveSize->SetHeightOverride(52.0f);
		ApproveButton->SetContent(ApproveSize);
		if (UButtonSlot* ApproveContentSlot = Cast<UButtonSlot>(ApproveSize->Slot))
		{
			ApproveContentSlot->SetPadding(FMargin(0.0f));
			ApproveContentSlot->SetHorizontalAlignment(HAlign_Fill);
			ApproveContentSlot->SetVerticalAlignment(VAlign_Fill);
		}
		ApproveSize->SetContent(MakeText(Tree, TEXT("Text_Approve"), TEXT("찬성"), 18, CPrimaryText, ETextJustify::Center, true, 18));

		UButton* RejectButton = NewWidget<UButton>(Tree, TEXT("Btn_Reject"));
		ApplyButtonStyle(RejectButton, CSecondary, CPrimaryHover, CPrimaryPressed);
		SetHBoxFill(ActionRow->AddChild(RejectButton), 1.0f, FMargin(8.0f, 0.0f, 0.0f, 0.0f));
		USizeBox* RejectSize = NewWidget<USizeBox>(Tree, TEXT("SB_Reject"));
		RejectSize->SetHeightOverride(52.0f);
		RejectButton->SetContent(RejectSize);
		if (UButtonSlot* RejectContentSlot = Cast<UButtonSlot>(RejectSize->Slot))
		{
			RejectContentSlot->SetPadding(FMargin(0.0f));
			RejectContentSlot->SetHorizontalAlignment(HAlign_Fill);
			RejectContentSlot->SetVerticalAlignment(VAlign_Fill);
		}
		RejectSize->SetContent(MakeText(Tree, TEXT("Text_Reject"), TEXT("반대"), 18, CPrimaryText, ETextJustify::Center, true, 18));
	}

	static UWidgetBlueprint* CreateWidgetBlueprint(const FString& PackagePath, const FString& AssetName, UClass* ParentClass)
	{
		const FString LongPackageName = PackagePath + TEXT("/") + AssetName;
		const FString ObjectPath = LongPackageName + TEXT(".") + AssetName;

		if (FindObject<UObject>(nullptr, *ObjectPath) || FPackageName::DoesPackageExist(LongPackageName))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(
				TEXT("%s가 이미 존재합니다. 기존 Auto 에셋을 삭제한 뒤 다시 실행하세요."),
				*AssetName)));
			return nullptr;
		}

		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = ParentClass;
		Factory->BlueprintType = BPTYPE_Normal;

		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, UWidgetBlueprint::StaticClass(), Factory);
		return Cast<UWidgetBlueprint>(NewAsset);
	}

	static bool FinalizeBlueprint(UWidgetBlueprint* Blueprint)
	{
		if (!Blueprint || !Blueprint->WidgetTree)
		{
			return false;
		}

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
		return true;
	}

	static void Generate()
	{
		const FString PackagePath = TEXT("/Game/UI/InGame/Menu");

		UWidgetBlueprint* RowBlueprint = CreateWidgetBlueprint(
			PackagePath,
			TEXT("WBP_SurrenderVoterRow_Auto"),
			UShooterSurrenderVoterRow::StaticClass());
		if (!RowBlueprint)
		{
			return;
		}
		if (RowBlueprint->WidgetTree->RootWidget)
		{
			RowBlueprint->WidgetTree->RemoveWidget(RowBlueprint->WidgetTree->RootWidget);
			RowBlueprint->WidgetTree->RootWidget = nullptr;
		}
		BuildVoterRowTree(RowBlueprint);
		if (!FinalizeBlueprint(RowBlueprint))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("WBP_SurrenderVoterRow_Auto 생성에 실패했습니다.")));
			return;
		}

		UWidgetBlueprint* ScreenBlueprint = CreateWidgetBlueprint(
			PackagePath,
			TEXT("WBP_SurrenderVote_Auto"),
			UShooterSurrenderVoteScreen::StaticClass());
		if (!ScreenBlueprint)
		{
			return;
		}
		if (ScreenBlueprint->WidgetTree->RootWidget)
		{
			ScreenBlueprint->WidgetTree->RemoveWidget(ScreenBlueprint->WidgetTree->RootWidget);
			ScreenBlueprint->WidgetTree->RootWidget = nullptr;
		}
		BuildMainTree(ScreenBlueprint);
		if (!FinalizeBlueprint(ScreenBlueprint))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("WBP_SurrenderVote_Auto 생성에 실패했습니다.")));
			return;
		}

		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
			TEXT("생성 완료:\n/Game/UI/InGame/Menu/WBP_SurrenderVote_Auto\n/Game/UI/InGame/Menu/WBP_SurrenderVoterRow_Auto\n\n실제 UI Init Component의 Surrender Vote Class에 WBP_SurrenderVote_Auto를 지정하세요.")));
	}

	static FAutoConsoleCommand GenerateCommand(
		TEXT("FPS.UI.GenerateSurrenderVoteWBP"),
		TEXT("Creates /Game/UI/InGame/Menu/WBP_SurrenderVote_Auto and WBP_SurrenderVoterRow_Auto."),
		FConsoleCommandDelegate::CreateStatic(&Generate));
}

#endif
