#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "Customization/CustomizationStudioController.h"
#include "Customization/CustomizationStudioActor.h"
#include "Misc/Paths.h"
#include "MuCO/CustomizableObject.h"
// Explicit opt-in developer smoke command. Never runs during ordinary gameplay.
static FAutoConsoleCommandWithWorld StudioSmoke(TEXT("FPS.Studio.Smoke"),TEXT("Capture standalone studio after mesh generation"),FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World){
 if(!World)return;auto Count=MakeShared<int32>(0);auto Handle=MakeShared<FTimerHandle>();TWeakObjectPtr<UWorld> Weak(World);
 World->GetTimerManager().SetTimer(*Handle,[Weak,Count,Handle](){if(!Weak.IsValid())return;auto* W=Weak.Get();++*Count;auto* PC=Cast<ACustomizationStudioController>(W->GetFirstPlayerController());
  if(*Count==5 && PC){auto* S=PC->Studio.Get();UE_LOG(LogTemp,Display,TEXT("STUDIO_SMOKE_STATE actor=%s status=%s controller=%s updating=%d compiled=%d tick=%d"),*GetNameSafe(S),S?*S->GetStatus():TEXT("none"),*PC->Status,S?S->IsUpdating():false,S&&S->Config&&S->Config->CustomizableObject?S->Config->CustomizableObject->IsCompiled():false,S?S->IsActorTickEnabled():false);FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/StudioInitial.png"),true,false);}
  if(PC&&PC->Studio&&!PC->Studio->IsUpdating()&&PC->Studio->GetAppliedRevision()>0&&*Count>5){UE_LOG(LogTemp,Display,TEXT("STUDIO_SMOKE_READY revision=%d status=%s"),PC->Studio->GetAppliedRevision(),*PC->Studio->GetStatus());FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/StudioSmoke.png"),true,false);W->GetTimerManager().ClearTimer(*Handle);FTimerHandle Exit;W->GetTimerManager().SetTimer(Exit,[](){FPlatformMisc::RequestExit(false);},4.f,false);}
  else if(*Count>90){UE_LOG(LogTemp,Error,TEXT("STUDIO_SMOKE_TIMEOUT"));W->GetTimerManager().ClearTimer(*Handle);FPlatformMisc::RequestExit(false);}
 },1.f,true);
}));
#endif
