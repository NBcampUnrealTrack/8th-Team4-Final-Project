#include "Customization/CustomizationStudioWidget.h"
#include "Customization/CustomizationStudioController.h"
#include "Customization/CustomizationStudioActor.h"
#include "Customization/CustomizationAppearanceControls.h"
#include "CustomizationPresentationTheme.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Font.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Colors/SColorBlock.h"

namespace MutableScreen
{
using CustomizationPresentationTheme::Accent;
using CustomizationPresentationTheme::Muted;
using CustomizationPresentationTheme::Ink;
class SModelViewport : public SBorder
{
public:
 TWeakObjectPtr<ACustomizationStudioActor> Studio;
 FKey DragButton;
 virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent& E) override
 {
  if(E.GetEffectingButton()==EKeys::LeftMouseButton||E.GetEffectingButton()==EKeys::RightMouseButton)
  {DragButton=E.GetEffectingButton();return FReply::Handled().CaptureMouse(SharedThis(this));}
  return FReply::Unhandled();
 }
 virtual FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent& E) override
 {
  if(E.GetEffectingButton()==DragButton){DragButton=FKey();return FReply::Handled().ReleaseMouseCapture();}
  return FReply::Unhandled();
 }
 virtual FReply OnMouseMove(const FGeometry& Geometry,const FPointerEvent& E) override
 {
  if(HasMouseCapture()&&Studio.IsValid()&&E.IsMouseButtonDown(DragButton))
  {
   if(DragButton==EKeys::RightMouseButton)
   {const FVector2D Delta=Geometry.AbsoluteToLocal(E.GetScreenSpacePosition())-Geometry.AbsoluteToLocal(E.GetLastScreenSpacePosition());Studio->PanPreview(Delta/FMath::Max(1.f,Geometry.GetLocalSize().Y));}
   else Studio->RotatePreview(-E.GetCursorDelta().X*.4f);
   return FReply::Handled();
  }
  return FReply::Unhandled();
 }
 virtual FReply OnMouseWheel(const FGeometry&,const FPointerEvent& E) override
 {
  if(Studio.IsValid())Studio->ZoomPreview(-E.GetWheelDelta()*18.f);
  return FReply::Handled();
 }
};
}

TSharedRef<SWidget> UCustomizationStudioWidget::BuildMutableLevelLayout()
{
 using namespace MutableScreen;
 Selected=0;bConfirmReset=false;
 // Keep styles alive with the widget. Neutral controls replace the old cyan textures.
 ActionStyle.SetNormal(FSlateRoundedBoxBrush(FLinearColor::White, 2.f, FLinearColor(.20f,.20f,.17f), 1.f));
 ActionStyle.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1.3f,1.3f,1.3f), 2.f, Accent, 1.f));
 ActionStyle.SetPressed(FSlateRoundedBoxBrush(FLinearColor(.7f,.7f,.7f), 2.f, Accent, 1.f));
 ActionStyle.SetDisabled(FSlateRoundedBoxBrush(FLinearColor(.45f,.45f,.45f), 2.f));
 PrimaryActionStyle=ActionStyle;
 const FSlateFontInfo TextFont=BodyFont?FSlateFontInfo(BodyFont,16):FCoreStyle::GetDefaultFontStyle("Regular",16);
 auto Label=[TextFont](const FString& Text,int32 Size=16,FLinearColor Color=FLinearColor::White)->TSharedRef<STextBlock>
 {
  auto Font=TextFont;Font.Size=Size;
  return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font).ColorAndOpacity(Color).AutoWrapText(true)
   .ShadowOffset(FVector2D(1,1)).ShadowColorAndOpacity(FLinearColor(0,0,0,.7f));
 };
 auto Action=[this,Label](const FString& Text,TFunction<void()> Fn)->TSharedRef<SButton>
 {
  auto Caption=Label(Text,15);Caption->SetAutoWrapText(false);Caption->SetColorAndOpacity(FSlateColor::UseForeground());
  return SNew(SButton).ButtonStyle(&ActionStyle).ButtonColorAndOpacity(CustomizationPresentationTheme::Button).ForegroundColor(CustomizationPresentationTheme::Text).ContentPadding(FMargin(14,10))
   .OnClicked_Lambda([Fn](){Fn();return FReply::Handled();})
   [SNew(SBox).MinDesiredHeight(22).VAlign(VAlign_Center).HAlign(HAlign_Center)[Caption]];
 };
 auto Section=[Label](const FString& Number,const FString& Name)->TSharedRef<SWidget>
 {
  return SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth().Padding(0,0,12,0)[Label(Number,14,Accent)]
   +SHorizontalBox::Slot().FillWidth(1)[Label(Name,18)];
 };
 auto Edit=SNew(SVerticalBox);
 auto Classes=SNew(SHorizontalBox);
 for(const FName Class:{FName(TEXT("Ranger")),FName(TEXT("Trooper")),FName(TEXT("Vanguard"))})
 {
  auto Caption=Label(Class.ToString().ToUpper(),13);Caption->SetAutoWrapText(false);Caption->SetColorAndOpacity(FSlateColor::UseForeground());
  auto Button=SNew(SButton).ButtonStyle(&ActionStyle).ContentPadding(FMargin(3,10)).HAlign(HAlign_Center)
   .OnClicked_Lambda([this,Class](){if(auto* C=Controller())C->SelectClass(Class);return FReply::Handled();})[Caption];
  Button->SetBorderBackgroundColor(TAttribute<FSlateColor>::CreateLambda([this,Class](){auto* C=Controller();return FSlateColor(C&&C->ActiveClass==Class?Accent:CustomizationPresentationTheme::Button);}));
  Button->SetForegroundColor(TAttribute<FSlateColor>::CreateLambda([this,Class](){auto* C=Controller();return FSlateColor(C&&C->ActiveClass==Class?Ink:CustomizationPresentationTheme::Text);}));
  Button->SetEnabled(TAttribute<bool>::CreateLambda([this,Class](){auto* C=Controller();return C&&C->Studio&&C->Studio->ClassConfigs.Contains(Class)&&!C->IsColorEditOpen();}));
  Classes->AddSlot().FillWidth(1).Padding(0,0,4,0)[Button];
 }
 Edit->AddSlot().AutoHeight().Padding(0,0,0,16)[Classes];
 Edit->AddSlot().AutoHeight().Padding(0,0,0,12)[Section(TEXT("01"),TEXT("헤어"))];
 Edit->AddSlot().AutoHeight().Padding(0,0,0,8)[Label(TEXT("몸·얼굴은 클래스 기본 외형으로 고정됩니다."),12,Muted)];
 // Build each class selector once. Visibility is live, so switching/load/reset
 // cannot leave callbacks referring to a previous class's body or hairstyle.
 for(const FName Class:{FName(TEXT("Ranger")),FName(TEXT("Trooper")),FName(TEXT("Vanguard"))})
 {
 UCustomizationStudioConfig* Config=nullptr;
 if(auto* C=Controller())if(C->Studio){if(auto* Entry=C->Studio->ClassConfigs.Find(Class))Config=Entry->Get();else if(C->Studio->Config&&C->Studio->Config->ClassId==Class.ToString())Config=C->Studio->Config;}
 if(!Config)continue;
 auto Selector=SNew(SVerticalBox).Visibility_Lambda([this,Class](){auto* C=Controller();return C&&C->ActiveClass==Class?EVisibility::Visible:EVisibility::Collapsed;});
 const TArray<FString> Types={FCustomizationAppearance::DefaultForClass(Class.ToString()).BodyType};
 // Each body has its own authored options. Visibility follows the draft, so
 // load/reset/body changes update the selector without losing saved hair IDs.
 for(const auto& Type:Types)
 {
  auto HairRow=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6,6))
   .Visibility_Lambda([this,Type](){auto* C=Controller();return C&&C->Draft.BodyType==Type?EVisibility::Visible:EVisibility::Collapsed;});
  TArray<FStudioGroomChoice> Choices;
  Choices=ACustomizationStudioActor::HairChoicesForConfig(Config,Type);
  TArray<FName> Ids={FName(TEXT("Bald"))};for(const auto& Choice:Choices)Ids.Add(Choice.Id);
  for(int32 I=0;I<Ids.Num();++I)
  {
   const FName Id=Ids[I];
   auto Button=Action(I==0?TEXT("X"):FString::FromInt(I),[this,Type,Id](){if(auto* C=Controller())if(C->Draft.BodyType==Type){C->Draft.HairId=Id;C->Changed();}});
   Button->SetToolTipText(FText::FromString(I==0?TEXT("헤어 없음"):FString::Printf(TEXT("헤어 %d"),I)));
   Button->SetBorderBackgroundColor(TAttribute<FSlateColor>::CreateLambda([this,Id](){auto* C=Controller();return FSlateColor(C&&C->Draft.HairId==Id?Accent:CustomizationPresentationTheme::Button);}));
   Button->SetForegroundColor(TAttribute<FSlateColor>::CreateLambda([this,Id](){auto* C=Controller();return FSlateColor(C&&C->Draft.HairId==Id?Ink:CustomizationPresentationTheme::Text);}));
   HairRow->AddSlot()[SNew(SBox).WidthOverride(76)[Button]];
  }
  Selector->AddSlot().AutoHeight().Padding(0,0,0,12)[HairRow];
 }
 Edit->AddSlot().AutoHeight()[Selector];
 }
 Edit->AddSlot().AutoHeight().Padding(0,0,0,12)[Section(TEXT("02"),TEXT("부위 및 색상"))];
 const TArray<FString> Parts={TEXT("상의"),TEXT("하의"),TEXT("조끼"),TEXT("부착물"),TEXT("벨트"),TEXT("장갑"),TEXT("부츠"),TEXT("스카프"),TEXT("피부"),TEXT("눈"),TEXT("헤어 색상")};
 auto PartRows=SNew(SVerticalBox);
 for(int32 Row=0;Row<6;++Row)
 {
  auto Pair=SNew(SHorizontalBox);
  for(int32 Col=0;Col<2;++Col)
  {
   int32 Index=Row*2+Col;
   if(!Parts.IsValidIndex(Index)){Pair->AddSlot().FillWidth(1)[SNullWidget::NullWidget];continue;}
   auto Button=Action(Parts[Index],[this,Index](){if(auto* C=Controller()){
    if(Index==3&&C->CanToggleAttachment()){C->ToggleAttachment();return;}
    C->BeginColorEdit(Index);if(C->IsColorEditOpen()){Selected=Index;ColorInputError.Reset();if(auto Popup=ColorPopup.Pin())FSlateApplication::Get().SetKeyboardFocus(Popup);}
   }});
   Button->SetEnabled(TAttribute<bool>::CreateLambda([this,Index](){auto* C=Controller();return C&&(C->CanEditPart(Index)||(Index==3&&C->CanToggleAttachment()));}));
   Button->SetBorderBackgroundColor(TAttribute<FSlateColor>::CreateLambda([this,Index](){auto* C=Controller();const bool Active=Index==3&&C&&C->CanToggleAttachment()?C->Draft.bAttachmentVisible:Selected==Index;return FSlateColor(Active?Accent:CustomizationPresentationTheme::Button);}));
   Button->SetForegroundColor(TAttribute<FSlateColor>::CreateLambda([this,Index](){auto* C=Controller();const bool Active=Index==3&&C&&C->CanToggleAttachment()?C->Draft.bAttachmentVisible:Selected==Index;return FSlateColor(Active?Ink:CustomizationPresentationTheme::Text);}));
   if(Index==3)
   {
    auto Caption=Label(TEXT("부착물"),15);
    Caption->SetAutoWrapText(false);
    Caption->SetColorAndOpacity(FSlateColor::UseForeground());
    Caption->SetText(TAttribute<FText>::CreateLambda([this](){auto* C=Controller();return FText::FromString(C&&(C->ActiveClass==TEXT("Ranger")||C->ActiveClass==TEXT("Vanguard"))?(C->Draft.bAttachmentVisible?TEXT("선글라스 ON"):TEXT("선글라스 OFF")):TEXT("부착물"));}));
    Button->SetContent(SNew(SBox).MinDesiredHeight(22).VAlign(VAlign_Center).HAlign(HAlign_Center)[Caption]);
   }
   Pair->AddSlot().FillWidth(1).Padding(0,0,4,4)[Button];
  }
  PartRows->AddSlot().AutoHeight()[Pair];
 }
 Edit->AddSlot().AutoHeight()[PartRows];
 Edit->AddSlot().AutoHeight().Padding(0,8)[Label(TEXT("부위: 색상 변경 · 선글라스: 착용/해제"),12,Muted)];

 auto Preview=SNew(SModelViewport).BorderImage(FCoreStyle::Get().GetBrush("NoBrush"));
 if(auto* C=Controller())Preview->Studio=C->Studio;
 auto HeaderFont=HeadingFont?FSlateFontInfo(HeadingFont,46):FCoreStyle::GetDefaultFontStyle("Bold",46);
 auto Left=SNew(SVerticalBox);
 Left->AddSlot().AutoHeight()[Label(TEXT("DUSTBASE  /  OPERATOR CUSTOMIZATION"),14,Accent)];
 Left->AddSlot().AutoHeight().Padding(0,6)[SNew(STextBlock).Font(HeaderFont).Text_Lambda([this](){auto* C=Controller();return FText::FromString(C?C->ActiveClass.ToString().ToUpper():TEXT(""));})];
 Left->AddSlot().AutoHeight()[Label(TEXT("장비를 정비하고, 나만의 스타일을 완성하세요."),16,CustomizationPresentationTheme::Text)];
 Left->AddSlot().FillHeight(1)[Preview];
 Left->AddSlot().AutoHeight().HAlign(HAlign_Right)
 [SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth().Padding(5)[Action(TEXT("전신 보기"),[this](){if(auto* C=Controller())if(C->Studio)C->Studio->SetFaceView(false);})]
  +SHorizontalBox::Slot().AutoWidth().Padding(5)[Action(TEXT("얼굴 보기"),[this](){if(auto* C=Controller())if(C->Studio)C->Studio->SetFaceView(true);})]];
 Left->AddSlot().AutoHeight().HAlign(HAlign_Right).Padding(0,8)
 [SNew(STextBlock).Font(TextFont).ColorAndOpacity(CustomizationPresentationTheme::Text).Justification(ETextJustify::Center)
  .ShadowOffset(FVector2D(1,1)).ShadowColorAndOpacity(FLinearColor::Black)
  .Text(FText::FromString(TEXT("좌클릭 회전 · 우클릭 이동\n휠 확대·축소")))];

 auto Save=Action(TEXT("외형 저장"),[this](){if(auto* C=Controller())C->SaveAppearance();});
 Save->SetButtonStyle(&PrimaryActionStyle);Save->SetBorderBackgroundColor(Accent);Save->SetForegroundColor(Ink);
 Save->SetEnabled(TAttribute<bool>::CreateLambda([this](){auto* C=Controller();return C&&C->CanSave();}));
 auto Right=SNew(SVerticalBox);
 Right->AddSlot().AutoHeight().Padding(0,0,0,6)[SNew(STextBlock).Font(HeadingFont?FSlateFontInfo(HeadingFont,30):TextFont).Text(FText::FromString(TEXT("CUSTOMIZE"))).ColorAndOpacity(CustomizationPresentationTheme::Text)];
 Right->AddSlot().AutoHeight().Padding(0,0,0,22)[Label(TEXT("헤어 · 의상 색상 · 부착물"),14,Muted)];
 Right->AddSlot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Edit]];
 Right->AddSlot().AutoHeight().Padding(0,12)
 [SNew(STextBlock).Font(TextFont).ColorAndOpacity(Muted).AutoWrapText(true)
  .Text_Lambda([this](){auto* C=Controller();return FText::FromString(C?(C->bDirty?TEXT("● 미저장 변경사항\n"):TEXT(""))+C->Status+TEXT("\n")+(C->Studio?C->Studio->GetStatus():TEXT("미리보기 없음")):TEXT("준비 중"));})];
 auto ConflictChoices=SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)[Action(TEXT("PC 저장 사용"),[this](){if(auto* C=Controller())C->ResolveCloudConflict(true);})]
  +SHorizontalBox::Slot().FillWidth(1)[Action(TEXT("클라우드 저장 사용"),[this](){if(auto* C=Controller())C->ResolveCloudConflict(false);})];
 ConflictChoices->SetVisibility(TAttribute<EVisibility>::CreateLambda([this](){auto* C=Controller();return C&&C->HasCloudConflict()?EVisibility::Visible:EVisibility::Collapsed;}));
 Right->AddSlot().AutoHeight().Padding(0,6)[ConflictChoices];
 Right->AddSlot().AutoHeight().Padding(0,8)[SNew(STextBlock).Font(TextFont).ColorAndOpacity(Muted).AutoWrapText(true)
  .Text_Lambda([this](){auto* C=Controller();return FText::FromString(C?C->CloudStatus():TEXT("저장 상태 확인 중"));})];
 auto Content=SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1).Padding(48,40,24,24)[Left]
  +SHorizontalBox::Slot().AutoWidth().Padding(0,24,28,24)
  [SNew(SBox).WidthOverride(440)
   [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
    .BorderBackgroundColor(CustomizationPresentationTheme::Panel).Padding(24)[Right]]];
 auto Footer=SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
  [SNew(STextBlock).Font(TextFont).ColorAndOpacity(Muted).AutoWrapText(true)
   .Text_Lambda([this](){auto* C=Controller();return FText::FromString(C?C->AccountLabel:TEXT("계정 확인 중"));})]
  +SHorizontalBox::Slot().AutoWidth().Padding(6,0)[Action(TEXT("불러오기"),[this](){if(auto* C=Controller())C->LoadAppearance();})]
  +SHorizontalBox::Slot().AutoWidth().Padding(6,0)[Action(TEXT("초기화"),[this](){bConfirmReset=true;})]
  +SHorizontalBox::Slot().AutoWidth().Padding(6,0)[Action(TEXT("뒤로"),[this](){if(auto* C=Controller())C->RequestReturnToFrontend();})]
  +SHorizontalBox::Slot().AutoWidth().Padding(6,0)[SNew(SBox).WidthOverride(190)[Save]];
 auto Main=SNew(SVerticalBox)
  +SVerticalBox::Slot().FillHeight(1)[Content]
  +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
   .BorderBackgroundColor(CustomizationPresentationTheme::Panel).Padding(40,16)[Footer]];
 auto Overlay=SNew(SOverlay)+SOverlay::Slot()[Main];
 Main->SetEnabled(TAttribute<bool>::CreateLambda([this](){auto* C=Controller();return !C||!C->IsColorEditOpen();}));
 Overlay->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0,16,484,16)[BuildMutableColorPopup()];
 Overlay->AddSlot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
 [SNew(SBorder).Visibility_Lambda([this](){return bConfirmReset?EVisibility::Visible:EVisibility::Collapsed;})
  .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0,0,0,.85f))
  .HAlign(HAlign_Center).VAlign(VAlign_Center)
  [SNew(SBox).WidthOverride(460)
   [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(CustomizationPresentationTheme::Panel).Padding(30)
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[Label(TEXT("외형을 초기화할까요?"),24)]
     +SVerticalBox::Slot().AutoHeight().Padding(0,18)[Label(TEXT("현재 편집값이 기본값으로 돌아갑니다.\n저장된 외형은 저장 버튼을 누르기 전까지 유지됩니다."),16,Muted)]
     +SVerticalBox::Slot().AutoHeight()
     [SNew(SHorizontalBox)
      +SHorizontalBox::Slot().FillWidth(1).Padding(4)[Action(TEXT("취소"),[this](){bConfirmReset=false;})]
      +SHorizontalBox::Slot().FillWidth(1).Padding(4)[Action(TEXT("초기화"),[this](){if(auto* C=Controller())C->ResetAppearance();bConfirmReset=false;})]]]]]];
 // Fixed design canvas scales down as a unit; the inspector content scrolls independently.
 return SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
  [SNew(SBox).WidthOverride(1600).HeightOverride(900)[Overlay]];
}
