#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Components/SkeletalMeshComponent.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "GroomComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Customization/CustomizationStudioActor.h"
#include "Customization/CustomizationStudioController.h"

// Opt-in, in-memory probe only. Does not write any appearance save slot.
static FAutoConsoleCommandWithWorld MutableProbe(TEXT("FPS.MutableLevel.Probe"),
 TEXT("Exercise latest generation, failed preview recovery and capture MutableLevel"),
 FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
 {
  if(!World || !World->GetMapName().Contains(TEXT("MutableLevel")))return;
  struct FProbe
  {
   int32 Phase=0,Seconds=0,Revision=0;
   TStrongObjectPtr<UCustomizationStudioConfig> Config;
   TStrongObjectPtr<USkeletalMesh> PreviousMesh;
   FCustomizationAppearance Original;
   FTimerHandle Timer;
  };
  auto Probe=MakeShared<FProbe>();TWeakObjectPtr<UWorld> Weak(World);
  World->GetTimerManager().SetTimer(Probe->Timer,[Weak,Probe]()
  {
   if(!Weak.IsValid())return;
   auto* W=Weak.Get();++Probe->Seconds;
   auto* C=Cast<ACustomizationStudioController>(W->GetFirstPlayerController());
   auto* S=C?C->Studio.Get():nullptr;
   auto Fail=[W,Probe](const TCHAR* Reason){UE_LOG(LogTemp,Error,TEXT("MUTABLE_PROBE_FAILED %s"),Reason);W->GetTimerManager().ClearTimer(Probe->Timer);FPlatformMisc::RequestExit(false);};
   auto Shot=[W](const TCHAR* Name){const auto Size=W->GetGameViewport()->Viewport->GetSizeXY();UE_LOG(LogTemp,Display,TEXT("MUTABLE_PROBE_CAPTURE %s %dx%d"),Name,Size.X,Size.Y);FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/FString::Printf(TEXT("%s_%dx%d.png"),Name,Size.X,Size.Y),true,false);};
   if(Probe->Seconds>120){Fail(TEXT("timeout"));return;}
   if(!S)return;
   if(Probe->Phase==0 && S->IsAppearanceReady() && Probe->Seconds>=6)
   {
    if(S->Face->GetForcedLOD()!=S->PreviewFaceLOD){Fail(TEXT("preview face detail setting was lost"));return;}
    Probe->Original=C->Draft;Probe->Revision=S->GetAppliedRevision();
    Shot(TEXT("MutableFull"));Probe->Phase=1;return;
   }
   if(Probe->Phase==1)
   {
    for(int32 I=0;I<10;++I){C->Draft.SkinValue=.5f+.05f*I;C->Changed();}
    if(C->CanSave()){Fail(TEXT("save allowed while update pending"));return;}
    Probe->Phase=2;return;
   }
   if(Probe->Phase==2 && S->IsAppearanceReady())
   {
    if(S->GetAppliedRevision()!=Probe->Revision+10){Fail(TEXT("stale generation became visible"));return;}
    UE_LOG(LogTemp,Display,TEXT("MUTABLE_PROBE_COALESCED_EDITS_PASS"));
    Probe->PreviousMesh.Reset(S->Body->GetSkeletalMeshAsset());Probe->Config.Reset(S->Config.Get());S->Config=nullptr;C->Changed();Probe->Phase=3;return;
   }
   if(Probe->Phase==3 && !S->IsUpdating())
   {
    if(C->CanSave() || S->Body->GetSkeletalMeshAsset()!=Probe->PreviousMesh.Get()){Fail(TEXT("rejected configuration lost prior mesh or allowed saving"));return;}
    UE_LOG(LogTemp,Display,TEXT("MUTABLE_PROBE_CONFIG_FAILURE_PRESERVED_PASS"));
    S->Config=Probe->Config.Get();C->Draft=Probe->Original;C->Changed();Probe->Phase=4;return;
   }
   if(Probe->Phase==4 && S->IsAppearanceReady())
   {
    if(!C->CanSave()){Fail(TEXT("save did not recover"));return;}
    S->SetFaceView(true);Probe->Phase=5;return;
   }
   if(Probe->Phase==5){Shot(TEXT("MutableFace"));Probe->Phase=6;return;}
   if(Probe->Phase==6){C->SelectBody(TEXT("Custom"));Probe->Phase=7;return;}
   if(Probe->Phase==7 && S->IsAppearanceReady()){Shot(TEXT("MutableCustomFace"));Probe->Phase=8;return;}
   if(Probe->Phase==8){C->SelectBody(TEXT("Fab"));Probe->Phase=9;return;}
   if(Probe->Phase==9 && S->IsAppearanceReady()){Shot(TEXT("MutableFabFace"));Probe->Phase=10;return;}
   if(Probe->Phase==10){C->Draft=Probe->Original;C->Changed();Probe->Phase=11;return;}
   if(Probe->Phase==11 && S->IsAppearanceReady()){S->SetFaceView(false);Probe->Phase=12;return;}
   if(Probe->Phase==12){Shot(TEXT("MutableRestoredFull"));Probe->Phase=13;return;}
   if(Probe->Phase==13 && FParse::Param(FCommandLine::Get(),TEXT("MutableDiagnoseFace")))
   {
    UE_LOG(LogTemp,Display,TEXT("MUTABLE_FACE_LOD forced=%d predicted=%d"),S->Face->GetForcedLOD(),S->Face->GetPredictedLODLevel());
    TInlineComponentArray<UGroomComponent*> Grooms(S);for(auto* G:Grooms)G->SetVisibility(false);
    Probe->Phase=14;return;
   }
   if(Probe->Phase==14){Shot(TEXT("MutableNoGrooms"));Probe->Phase=15;return;}
   if(Probe->Phase==15){S->Body->SetVisibility(false);S->Outfit->SetVisibility(false);Probe->Phase=16;return;}
   if(Probe->Phase==16){Shot(TEXT("MutableFaceOnly"));Probe->Phase=17;return;}
   if(Probe->Phase==13 || Probe->Phase==17){UE_LOG(LogTemp,Display,TEXT("MUTABLE_PROBE_PASSED"));W->GetTimerManager().ClearTimer(Probe->Timer);FPlatformMisc::RequestExit(false);}
  },1.f,true);
 }));
#endif
