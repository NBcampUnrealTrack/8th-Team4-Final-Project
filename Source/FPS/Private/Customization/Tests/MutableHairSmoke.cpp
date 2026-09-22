#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "GroomComponent.h"
#include "GroomBindingAsset.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Customization/CustomizationStudioActor.h"
#include "Customization/CustomizationStudioController.h"

// Checks real authored bindings, including cross-body hair choices; never saves a profile.
static FAutoConsoleCommandWithWorld MutableHairProbe(TEXT("FPS.MutableLevel.HairProbe"),
 TEXT("Verify all body/hair bindings and capture each combination, then exit"),
 FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
 {
  if(!World || !World->GetMapName().Contains(TEXT("MutableLevel")))return;
  struct FState {int32 Seconds=0,Index=-1,Phase=0;TArray<FStudioGroomChoice> Cases;FTimerHandle Timer;};
  auto State=MakeShared<FState>();TWeakObjectPtr<UWorld> Weak(World);
  World->GetTimerManager().SetTimer(State->Timer,[Weak,State]()
  {
   if(!Weak.IsValid())return;
   auto* W=Weak.Get();auto* C=Cast<ACustomizationStudioController>(W->GetFirstPlayerController());
   auto* S=C?C->Studio.Get():nullptr;
   auto Finish=[W,State](bool Pass,const TCHAR* Why){UE_LOG(LogTemp,Display,TEXT("MUTABLE_HAIR_%s %s"),Pass?TEXT("PASS"):TEXT("FAIL"),Why);W->GetTimerManager().ClearTimer(State->Timer);FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);};
   if(++State->Seconds>180){Finish(false,TEXT("timeout"));return;}
   if(!S || !S->IsAppearanceReady())return;
   if(State->Phase==0)
   {
    // User's failing combination first, then every remaining configured choice.
    for(const auto& Type:{TEXT("Fab"),TEXT("Seo"),TEXT("Custom")})
    {
     for(const auto& Choice:S->HairChoices(Type))State->Cases.Add(Choice);
     FStudioGroomChoice Bald;Bald.BodyType=Type;Bald.Id=TEXT("Bald");State->Cases.Add(Bald);
    }
    if(State->Cases.IsEmpty()){Finish(false,TEXT("no choices"));return;}
    S->SetFaceView(true);S->RotatePreview(-18);State->Phase=1;
   }
   if(State->Phase==1)
   {
    if(++State->Index>=State->Cases.Num()){Finish(true,TEXT("all combinations bound to authored targets"));return;}
    const auto& Choice=State->Cases[State->Index];
    C->Draft.BodyType=Choice.BodyType;C->Draft.HairId=Choice.Id;C->Changed();State->Phase=2;return;
   }
   if(State->Phase==2){State->Phase=3;return;} // settle render/streaming after generation
   if(State->Phase==3)
   {
    const auto& Choice=State->Cases[State->Index];
    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/FString::Printf(TEXT("MutableHair_%s_%s.png"),*Choice.BodyType,*Choice.Id.ToString()),true,false);
    TInlineComponentArray<UGroomComponent*> Grooms(S);int32 HairCount=0;
    for(auto* G:Grooms)if(G->GetFName()==TEXT("Hair"))
    {
     ++HairCount;
     auto* Surface=Cast<USkeletalMeshComponent>(G->GetAttachParent());
     const bool Valid=Choice.Id==TEXT("Bald")?(!G->IsVisible()&&!G->GroomAsset):
      (G->IsVisible()&&G->GroomAsset==Choice.Groom&&G->BindingAsset==Choice.Binding&&Surface&&Surface->GetSkeletalMeshAsset()==Choice.Binding->GetTargetSkeletalMesh());
     if(!Valid)
     {UE_LOG(LogTemp,Error,TEXT("MUTABLE_HAIR_WRONG_SURFACE %s %s"),*Choice.BodyType,*Choice.Id.ToString());State->Phase=4;return;}
     UE_LOG(LogTemp,Display,TEXT("MUTABLE_HAIR_CASE_PASS %s %s"),*Choice.BodyType,*Choice.Id.ToString());
    }
    if(HairCount!=1){Finish(false,TEXT("expected one Hair component"));return;}
    State->Phase=1;return;
   }
   if(State->Phase==4)Finish(false,TEXT("hair binding lost or target topology mismatched"));
  },1.f,true);
 }));
#endif
