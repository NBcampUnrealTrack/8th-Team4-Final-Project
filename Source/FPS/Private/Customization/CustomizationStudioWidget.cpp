#include "Customization/CustomizationStudioWidget.h"
#include "Customization/CustomizationStudioController.h"
#include "Customization/CustomizationStudioActor.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Framework/Application/SlateApplication.h"

namespace StudioUI {
class SPreviewBorder : public SBorder
{
public:
 TFunction<void(float)> Wheel;
 virtual FReply OnMouseWheel(const FGeometry&,const FPointerEvent& Event) override {if(Wheel)Wheel(Event.GetWheelDelta());return FReply::Handled();}
};
static TSharedRef<SWidget> Label(const FString& Text,int Size=14){return SNew(STextBlock).Text(FText::FromString(Text)).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size)).AutoWrapText(true);}
static TSharedRef<SWidget> Button(const FString& Text,TFunction<void()> Action){return SNew(SButton).ContentPadding(FMargin(12,8)).OnClicked_Lambda([Action](){Action();return FReply::Handled();})[Label(Text)];}
}
ACustomizationStudioController* UCustomizationStudioWidget::Controller() const{return Cast<ACustomizationStudioController>(GetOwningPlayer());}
FLinearColor UCustomizationStudioWidget::CurrentColor() const{auto* C=Controller();if(!C)return FLinearColor::White;return Selected==8?C->Draft.SkinColor:Selected==9?C->Draft.EyeColor:C->Draft.Colors[Selected];}
void UCustomizationStudioWidget::SetColor(FLinearColor Color){if(auto* C=Controller()){Color.A=1;if(Selected==8)C->Draft.SkinColor=Color;else if(Selected==9)C->Draft.EyeColor=Color;else{C->Draft.Colors[Selected]=Color;C->Draft.Originals[Selected]=0;}C->Changed();}}
TSharedRef<SWidget> UCustomizationStudioWidget::HairMenu()
{
 auto List=SNew(SVerticalBox);auto* C=Controller();
 List->AddSlot().AutoHeight()[StudioUI::Button(TEXT("머리 없음"),[this](){if(auto* P=Controller()){P->Draft.HairId=TEXT("Bald");P->Changed();FSlateApplication::Get().DismissAllMenus();}})];
 if(C&&C->Studio)for(auto H:C->Studio->HairChoices(C->Draft.BodyType))List->AddSlot().AutoHeight()[StudioUI::Button(H.Label,[this,H](){if(auto* P=Controller()){P->Draft.HairId=H.Id;P->Changed();FSlateApplication::Get().DismissAllMenus();}})];
 return List;
}
TSharedRef<SWidget> UCustomizationStudioWidget::RebuildWidget()
{
 using namespace StudioUI;
 auto Panel=SNew(SVerticalBox);
 Panel->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("RANGER / CUSTOMIZATION"),22)];
 Panel->AddSlot().AutoHeight()[SNew(STextBlock).Text_Lambda([this](){auto* C=Controller();return FText::FromString(C?C->AccountLabel:TEXT(""));}).AutoWrapText(true)];
 Panel->AddSlot().AutoHeight().Padding(0,14,0,4)[Label(TEXT("캐릭터 외형"),16)];
 auto Bodies=SNew(SHorizontalBox);for(FString Type:{TEXT("Seo"),TEXT("Custom"),TEXT("Fab")})Bodies->AddSlot().FillWidth(1).Padding(2)[Button(Type,[this,Type](){if(auto* C=Controller())C->SelectBody(Type);})];
 Panel->AddSlot().AutoHeight()[Bodies];
 Panel->AddSlot().AutoHeight().Padding(0,6)[SNew(STextBlock).Text_Lambda([this](){auto* C=Controller();return FText::FromString(C?TEXT("선택: ")+C->Draft.BodyType:TEXT(""));})];
 Panel->AddSlot().AutoHeight()[SNew(SComboButton).OnGetMenuContent_Lambda([this](){return HairMenu();}).ButtonContent()[SNew(STextBlock).Text_Lambda([this](){auto* C=Controller();return FText::FromString(C?TEXT("헤어 선택 · ")+C->Draft.HairId.ToString():TEXT("헤어"));})]];
 Panel->AddSlot().AutoHeight().Padding(0,18,0,6)[Label(TEXT("편집할 부위"),16)];
 const TArray<FString> Labels={TEXT("상의"),TEXT("하의"),TEXT("조끼"),TEXT("부착물"),TEXT("벨트"),TEXT("장갑"),TEXT("부츠"),TEXT("스카프"),TEXT("피부"),TEXT("눈")};
 Panel->AddSlot().AutoHeight()[SNew(SComboButton).OnGetMenuContent_Lambda([this,Labels](){auto L=SNew(SVerticalBox);for(int K=0;K<Labels.Num();++K)L->AddSlot().AutoHeight()[StudioUI::Button(Labels[K],[this,K](){Selected=K;FSlateApplication::Get().DismissAllMenus();})];return L;}).ButtonContent()[SNew(STextBlock).Text_Lambda([this,Labels](){return FText::FromString(Labels[Selected]);})]];
 Panel->AddSlot().AutoHeight().Padding(0,12)[SNew(SColorBlock).Color_Lambda([this](){return CurrentColor();}).Size(FVector2D(340,40))];
 for(int Channel=0;Channel<3;++Channel){Panel->AddSlot().AutoHeight().Padding(0,4)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,10,0)[Label(Channel==0?TEXT("R"):Channel==1?TEXT("G"):TEXT("B"))]+SHorizontalBox::Slot().FillWidth(1)[SNew(SSpinBox<float>).MinValue(0).MaxValue(1).Delta(.01f).Value_Lambda([this,Channel](){return CurrentColor().Component(Channel);}).OnValueChanged_Lambda([this,Channel](float V){FLinearColor Color=CurrentColor();Color.Component(Channel)=V;SetColor(Color);})]];}
 Panel->AddSlot().AutoHeight().Padding(0,6)[SNew(SEditableTextBox).HintText(FText::FromString(TEXT("HEX 색상 #RRGGBB 입력 후 Enter"))).OnTextCommitted_Lambda([this](const FText& Text,ETextCommit::Type Commit){if(Commit!=ETextCommit::OnEnter)return;FString Hex=Text.ToString();Hex.RemoveFromStart(TEXT("#"));bool OK=Hex.Len()==6;for(TCHAR Ch:Hex)OK&=FChar::IsHexDigit(Ch);if(OK)SetColor(FLinearColor::FromSRGBColor(FColor::FromHex(Hex)));else if(auto* C=Controller())C->Status=TEXT("HEX는 6자리 색상값을 입력하세요.");})];
 Panel->AddSlot().AutoHeight().Padding(0,8)[SNew(SCheckBox).Visibility_Lambda([this](){return Selected<8?EVisibility::Visible:EVisibility::Collapsed;}).IsChecked_Lambda([this](){auto* C=Controller();return C&&Selected<8&&C->Draft.Originals[Selected]>=.5f?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState S){if(auto* C=Controller())if(Selected<8){C->Draft.Originals[Selected]=S==ECheckBoxState::Checked?1.f:0.f;C->Changed();}})[Label(TEXT("원본 텍스처 색 사용"))]];
 Panel->AddSlot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){return Selected!=9?EVisibility::Visible:EVisibility::Collapsed;})[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,0,10,0)[Label(TEXT("밝기"))]+SHorizontalBox::Slot().FillWidth(1)[SNew(SSpinBox<float>).MinValue_Lambda([this](){return TOptional<float>(Selected==8?.15f:0.f);}).MaxValue_Lambda([this](){return TOptional<float>(Selected==8?1.35f:2.f);}).Delta(.01f).Value_Lambda([this](){auto* C=Controller();return !C?1.f:Selected==8?C->Draft.SkinValue:Selected<8?C->Draft.Values[Selected]:1.f;}).OnValueChanged_Lambda([this](float V){if(auto* C=Controller()){if(Selected==8)C->Draft.SkinValue=V;else if(Selected<8)C->Draft.Values[Selected]=V;C->Changed();}})]]];
 Panel->AddSlot().AutoHeight().Padding(0,10)[Button(TEXT("이 부위 원본 복원"),[this](){if(auto* C=Controller()){if(Selected<8){C->Draft.Originals[Selected]=1;C->Draft.Values[Selected]=1;}else if(Selected==8){C->Draft.SkinColor=FLinearColor::White;C->Draft.SkinValue=1;}else C->Draft.EyeColor=FLinearColor::White;C->Changed();}})];
 Panel->AddSlot().AutoHeight()[Label(TEXT("색을 선택하면 의상은 지정색 모드로 전환됩니다.\n드래그: 회전 / 휠: 확대·축소"),12)];
 Panel->AddSlot().AutoHeight().Padding(0,15)[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).Padding(2)[Button(TEXT("저장"),[this](){if(auto* C=Controller())C->SaveAppearance();})]+SHorizontalBox::Slot().FillWidth(1).Padding(2)[Button(TEXT("불러오기"),[this](){if(auto* C=Controller())C->LoadAppearance();})]];
 Panel->AddSlot().AutoHeight()[Button(TEXT("전체 초기화 (저장 전까지 임시)"),[this](){if(auto* C=Controller())C->ResetAppearance();})];
 Panel->AddSlot().AutoHeight().Padding(0,12)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this](){auto* C=Controller();return FText::FromString(C?(C->bDirty?TEXT("● 미저장\n"):TEXT(""))+C->Status+TEXT("\n")+(C->Studio?C->Studio->GetStatus():TEXT("프리뷰 없음")):TEXT(""));})];
 auto Preview=SNew(SPreviewBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
 .OnMouseButtonDown_Lambda([this](const FGeometry&,const FPointerEvent& E){if(E.GetEffectingButton()==EKeys::LeftMouseButton){bDragging=true;return FReply::Handled();}return FReply::Unhandled();})
 .OnMouseButtonUp_Lambda([this](const FGeometry&,const FPointerEvent&){bDragging=false;return FReply::Handled();})
 .OnMouseMove_Lambda([this](const FGeometry&,const FPointerEvent& E){if(bDragging&&E.IsMouseButtonDown(EKeys::LeftMouseButton)){if(auto* C=Controller())if(C->Studio)C->Studio->RotatePreview(-E.GetCursorDelta().X*.4f);return FReply::Handled();}bDragging=false;return FReply::Unhandled();});
 Preview->Wheel=[this](float Delta){if(auto* C=Controller())if(C->Studio)C->Studio->ZoomPreview(-Delta*18);};
 Preview->SetContent(SNew(SVerticalBox)+SVerticalBox::Slot().FillHeight(1)[SNullWidget::NullWidget]+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(20)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(5)[Button(TEXT("전신"),[this](){if(auto* C=Controller())if(C->Studio)C->Studio->SetFaceView(false);})]+SHorizontalBox::Slot().AutoWidth().Padding(5)[Button(TEXT("얼굴"),[this](){if(auto* C=Controller())if(C->Studio)C->Studio->SetFaceView(true);})]]);
 return SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Preview]+SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(410)[SNew(SBorder).BorderBackgroundColor(FLinearColor(.035f,.045f,.06f,.97f)).Padding(22)[SNew(SScrollBox)+SScrollBox::Slot()[Panel]]]];
}
