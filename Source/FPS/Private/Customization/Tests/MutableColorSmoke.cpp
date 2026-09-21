#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Customization/CustomizationStudioActor.h"
#include "Customization/CustomizationStudioController.h"
#include "Customization/CustomizationStudioWidget.h"
#include "GroomComponent.h"
#include "GroomAsset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace MutableColorSmoke
{
TSharedPtr<SWidget> Find(const TSharedRef<SWidget>& Root,TFunctionRef<bool(const SWidget&)> Match)
{
 if(!Root->GetVisibility().IsVisible())return nullptr;
 if(Match(Root.Get()))return Root;
 auto* Children=Root->GetChildren();
 for(int32 I=0;I<Children->Num();++I)if(auto W=Find(Children->GetChildAt(I),Match))return W;
 return nullptr;
}
bool Click(UWorld* W,const FString& Caption)
{
 TArray<UUserWidget*> Screens;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Screens,UCustomizationStudioWidget::StaticClass(),true);
 if(Screens.Num()!=1)return false;
 auto Found=Find(Screens[0]->TakeWidget(),[&](const SWidget& Widget){
  if(Widget.GetType()!=FName(TEXT("SButton")))return false;
  return Find(ConstCastSharedRef<SWidget>(Widget.AsShared()),[&](const SWidget& Child){return Child.GetType()==FName(TEXT("STextBlock"))&&static_cast<const STextBlock&>(Child).GetText().ToString()==Caption;}).IsValid();
 });
 if(!Found)return false;StaticCastSharedPtr<SButton>(Found)->SimulateClick();return true;
}
}

// Real game rendering + actual Slate button callbacks. Never writes a user profile.
static FAutoConsoleCommandWithWorld MutableColorProbe(TEXT("FPS.MutableLevel.ColorProbe"),TEXT("Check hair dye and popup, capture, then exit"),
 FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
 {
  if(!World||!World->GetMapName().Contains(TEXT("MutableLevel")))return;
  struct FState{FTimerHandle Timer;int32 Seconds=0,Index=-1,Phase=0;TArray<FStudioGroomChoice> Cases;FLinearColor Color;};
  auto State=MakeShared<FState>();TWeakObjectPtr<UWorld> Weak(World);
  World->GetTimerManager().SetTimer(State->Timer,[Weak,State]()
  {
   if(!Weak.IsValid())return;auto* W=Weak.Get();auto* C=Cast<ACustomizationStudioController>(W->GetFirstPlayerController());auto* S=C?C->Studio.Get():nullptr;
   auto Finish=[W,State](bool OK,const TCHAR* Why){UE_LOG(LogTemp,Display,TEXT("MUTABLE_COLOR_%s %s"),OK?TEXT("PASS"):TEXT("FAIL"),Why);W->GetTimerManager().ClearTimer(State->Timer);FPlatformMisc::RequestExitWithStatus(false,OK?0:1);};
   if(++State->Seconds>180){Finish(false,TEXT("timeout"));return;}
   if(!S||!S->IsAppearanceReady())return;
   if(State->Phase==0)
   {
    for(const auto Type:{TEXT("Seo"),TEXT("Custom"),TEXT("Fab")})for(const auto& H:S->HairChoices(Type))State->Cases.Add(H);
    S->SetFaceView(true);State->Phase=1;
   }
   if(State->Phase==1)
   {
    if(++State->Index>=State->Cases.Num()){Finish(true,TEXT("nine styles, dye and original restore"));return;}
    const auto& H=State->Cases[State->Index];C->Draft.BodyType=H.BodyType;C->Draft.HairId=H.Id;C->Changed();State->Phase=2;return;
   }
   if(State->Phase==2)
   {
    if(!MutableColorSmoke::Click(W,TEXT("헤어 색상"))||!C->IsColorEditOpen()){Finish(false,TEXT("hair button did not open popup"));return;}
    const FLinearColor Colors[]={FLinearColor(.03f,.12f,.7f),FLinearColor(.7f,.12f,.03f),FLinearColor(.65f,.65f,.65f)};
    State->Color=Colors[State->Index%3];C->SetEditingColor(State->Color);State->Phase=3;return;
   }
   if(State->Phase==3){State->Phase=4;return;}
   if(State->Phase==4)
   {
    TInlineComponentArray<UGroomComponent*> Grooms(S);bool HasHair=false;
    for(auto* G:Grooms)if(G->GetFName()==TEXT("Hair"))
    {
     HasHair=G->GroomAsset!=nullptr&&G->IsVisible();if(!HasHair)continue;
     const auto& Materials=G->GroomAsset->GetHairGroupsMaterials();
     for(int32 I=0;I<Materials.Num();++I)
     {
      auto* MID=Cast<UMaterialInstanceDynamic>(G->GetMaterial(I));
      if(!MID||MID->K2_GetVectorParameterValue(TEXT("hairDye"))!=State->Color){Finish(false,TEXT("dye missing from material slot"));return;}
     }
    }
    if(!HasHair){Finish(false,TEXT("hair not visible"));return;}
    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/FString::Printf(TEXT("MutableColor_%d.png"),State->Index),true,false);
    State->Phase=5;return;
   }
   if(State->Phase==5)
   {
    if(!MutableColorSmoke::Click(W,TEXT("원본 색 복원"))){Finish(false,TEXT("restore missing"));return;}
    State->Phase=6;return;
   }
   if(State->Phase==6)
   {
    TInlineComponentArray<UGroomComponent*> Grooms(S);
    for(auto* G:Grooms)if(G->GetFName()==TEXT("Hair"))for(int32 I=0;I<G->GetNumMaterials();++I)
     if(Cast<UMaterialInstanceDynamic>(G->GetMaterial(I))){Finish(false,TEXT("original left stale dye"));return;}
    if(!MutableColorSmoke::Click(W,TEXT("확인"))||C->IsColorEditOpen()){Finish(false,TEXT("confirm did not close"));return;}
    UE_LOG(LogTemp,Display,TEXT("MUTABLE_COLOR_CASE_PASS %d"),State->Index);State->Phase=1;
   }
  },1.f,true);
 }));
#endif
