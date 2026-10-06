#include "Customization/CustomizationStudioWidget.h"
#include "Customization/CustomizationStudioController.h"
#include "Customization/CustomizationAppearanceControls.h"
#include "CustomizationPresentationTheme.h"
#include "Engine/Font.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorWheel.h"

namespace
{
class SMutableColorPanel : public SBorder
{
public:
 TWeakObjectPtr<ACustomizationStudioController> Controller;
 virtual bool SupportsKeyboardFocus() const override{return true;}
 virtual FReply OnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override
 {
  if(Event.GetKey()==EKeys::Escape&&Controller.IsValid()){Controller->EndColorEdit(false);return FReply::Handled();}
  return SBorder::OnPreviewKeyDown(Geometry,Event);
 }
};
}

TSharedRef<SWidget> UCustomizationStudioWidget::BuildMutableColorPopup()
{
 const FLinearColor Accent=CustomizationPresentationTheme::Accent;
 const auto Font=BodyFont?FSlateFontInfo(BodyFont,15):FCoreStyle::GetDefaultFontStyle("Regular",15);
 auto Label=[Font](const FString& Text){return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font);};
 auto Action=[this,Label](const FString& Text,TFunction<void()> Fn){return SNew(SButton).ButtonStyle(&ActionStyle).ButtonColorAndOpacity(CustomizationPresentationTheme::Button).ContentPadding(FMargin(10,8))
  .OnClicked_Lambda([Fn](){Fn();return FReply::Handled();})[SNew(SBox).HAlign(HAlign_Center)[Label(Text)]];};
 auto GetColor=[this](){auto* C=Controller();return C?C->GetEditingColor():FLinearColor::White;};
 auto Panel=SNew(SVerticalBox);
 Panel->AddSlot().AutoHeight().Padding(0,0,0,14)
 [SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
  [SNew(STextBlock).Font(Font).ColorAndOpacity(Accent).Text_Lambda([this](){
   const TArray<FString> Names={TEXT("상의"),TEXT("하의"),TEXT("조끼"),TEXT("부착물"),TEXT("벨트"),TEXT("장갑"),TEXT("부츠"),TEXT("스카프"),TEXT("피부"),TEXT("눈"),TEXT("헤어")};
   auto* C=Controller();const int32 Part=C?C->GetColorEditPart():INDEX_NONE;
   return FText::FromString((Names.IsValidIndex(Part)?Names[Part]:TEXT(""))+TEXT(" / 색상 선택"));
  })]
  +SHorizontalBox::Slot().AutoWidth()[Action(TEXT("닫기"),[this](){if(auto* C=Controller())C->EndColorEdit(false);})]];
 Panel->AddSlot().AutoHeight().HAlign(HAlign_Center)
 [SNew(SBox).WidthOverride(210).HeightOverride(210)
  [SNew(SColorWheel).SelectedColor_Lambda([GetColor](){return GetColor().LinearRGBToHSV();})
   .OnValueChanged_Lambda([this](FLinearColor HSV){ColorInputError.Reset();if(auto* C=Controller())C->SetEditingColor(HSV.HSVToLinearRGB());})]];
 Panel->AddSlot().AutoHeight().Padding(0,12,0,4)[Label(TEXT("색상 밝기"))];
 Panel->AddSlot().AutoHeight()
 [SNew(SSlider).SliderHandleColor(Accent).Value_Lambda([GetColor](){return GetColor().LinearRGBToHSV().B;})
  .OnValueChanged_Lambda([this,GetColor](float V){ColorInputError.Reset();auto HSV=GetColor().LinearRGBToHSV();HSV.B=V;if(auto* C=Controller())C->SetEditingColor(HSV.HSVToLinearRGB());})];
 Panel->AddSlot().AutoHeight().Padding(0,12)
 [SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,5,0)
  [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Label(TEXT("변경 전"))]+SVerticalBox::Slot().AutoHeight()
   [SNew(SColorBlock).Color_Lambda([this](){auto* C=Controller();return C?C->GetPreviousColor():FLinearColor::White;}).Size(FVector2D(175,24))]]
  +SHorizontalBox::Slot().FillWidth(1)
  [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Label(TEXT("선택 색상"))]+SVerticalBox::Slot().AutoHeight()
   [SNew(SColorBlock).Color_Lambda(GetColor).Size(FVector2D(175,24))]]];
 for(int32 Channel=0;Channel<3;++Channel)
 {
  Panel->AddSlot().AutoHeight().Padding(0,3)
  [SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,12,0)[Label(Channel==0?TEXT("R"):Channel==1?TEXT("G"):TEXT("B"))]
   +SHorizontalBox::Slot().FillWidth(1)
   [SNew(SSpinBox<float>).Font(Font).MinValue(0.f).MaxValue(1.f).Delta(.01f)
    .Value_Lambda([GetColor,Channel](){return GetColor().Component(Channel);})
    .OnValueChanged_Lambda([this,GetColor,Channel](float Value){ColorInputError.Reset();auto Color=GetColor();Color.Component(Channel)=Value;if(auto* C=Controller())C->SetEditingColor(Color);})]];
 }
 Panel->AddSlot().AutoHeight().Padding(0,8)
 [SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,12,0)[Label(TEXT("HEX sRGB"))]
  +SHorizontalBox::Slot().FillWidth(1)
  [SNew(SEditableTextBox).Font(Font).Text_Lambda([GetColor](){return FText::FromString(TEXT("#")+GetColor().ToFColorSRGB().ToHex().Left(6));})
   .OnTextCommitted_Lambda([this](const FText& Text,ETextCommit::Type Type){
    if(Type!=ETextCommit::OnEnter&&Type!=ETextCommit::OnUserMovedFocus)return;
    FString Hex=Text.ToString().TrimStartAndEnd();Hex.RemoveFromStart(TEXT("#"));bool Valid=Hex.Len()==6;
    for(TCHAR Ch:Hex)Valid&=FChar::IsHexDigit(Ch);
    if(Valid){ColorInputError.Reset();if(auto* C=Controller()){
     // Slate commits on any focus loss, including a read-only visit to this box.
     // Preserve original-material mode and full precision when text is unchanged.
     if(!Hex.Equals(C->GetEditingColor().ToFColorSRGB().ToHex().Left(6),ESearchCase::IgnoreCase))
      C->SetEditingColor(FLinearColor::FromSRGBColor(FColor::FromHex(Hex)));
    }}
    else ColorInputError=TEXT("HEX는 6자리 색상값을 입력하세요.");
   })]];
 Panel->AddSlot().AutoHeight()
 [SNew(STextBlock).Font(Font).ColorAndOpacity(FLinearColor(1.f,.2f,.1f))
  .Visibility_Lambda([this](){return ColorInputError.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible;})
  .Text_Lambda([this](){return FText::FromString(ColorInputError);})];
 Panel->AddSlot().AutoHeight().Padding(0,8)
 [SNew(SCheckBox).Visibility_Lambda([this](){auto* C=Controller();return C&&C->GetColorEditPart()>=0&&C->GetColorEditPart()<8?EVisibility::Visible:EVisibility::Collapsed;})
  .IsChecked_Lambda([this](){auto* C=Controller();const int32 P=C?C->GetColorEditPart():INDEX_NONE;return C&&C->Draft.Originals.IsValidIndex(P)&&C->Draft.Originals[P]>=.5f?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
  .OnCheckStateChanged_Lambda([this](ECheckBoxState State){if(auto* C=Controller()){const int32 P=C->GetColorEditPart();if(C->Draft.Originals.IsValidIndex(P)){C->Draft.Originals[P]=State==ECheckBoxState::Checked?1.f:0.f;C->Changed();}}})
  [Label(TEXT("원본 텍스처 색 유지"))]];
 Panel->AddSlot().AutoHeight().Padding(0,6)
 [SNew(SVerticalBox).Visibility_Lambda([this](){auto* C=Controller();return C&&C->GetColorEditPart()>=0&&C->GetColorEditPart()<=8?EVisibility::Visible:EVisibility::Collapsed;})
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)[Label(TEXT("재질 밝기"))]
  +SVerticalBox::Slot().AutoHeight()
  [SNew(SSlider).SliderHandleColor(Accent).Value_Lambda([this](){auto* C=Controller();return C?MutableBrightness(C->Draft,C->GetColorEditPart()):0.f;})
   .OnValueChanged_Lambda([this](float V){if(auto* C=Controller()){const int32 P=C->GetColorEditPart();if(P==8)C->Draft.SkinValue=.15f+V*1.2f;else if(C->Draft.Values.IsValidIndex(P))C->Draft.Values[P]=2.f*V;else return;C->Changed();}})]];
 Panel->AddSlot().AutoHeight().Padding(0,10)[Action(TEXT("원본 색 복원"),[this](){ColorInputError.Reset();if(auto* C=Controller())C->RestoreEditingPart();})];
 Panel->AddSlot().AutoHeight()
 [SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,8,0)[Action(TEXT("확인"),[this](){if(ColorInputError.IsEmpty())if(auto* C=Controller())C->EndColorEdit(true);})]
  +SHorizontalBox::Slot().FillWidth(1)[Action(TEXT("취소"),[this](){if(auto* C=Controller())C->EndColorEdit(false);})]];
 auto Popup=SNew(SMutableColorPanel).Padding(22)
  .Visibility_Lambda([this](){auto* C=Controller();return C&&C->IsColorEditOpen()?EVisibility::Visible:EVisibility::Collapsed;})
  .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(CustomizationPresentationTheme::Panel)
  [SNew(SBox).WidthOverride(366)[Panel]];
 Popup->Controller=Controller();ColorPopup=Popup;
 return Popup;
}
