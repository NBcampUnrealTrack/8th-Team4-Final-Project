#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Data/WeaponData.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tags/GameplayTags.h"
namespace ShortHolsterTemp {
static const FString Root=TEXT("/Game/MetaHumans/Anim/FirstAnim/");
static bool Is1P(UEdGraphNode* N)
{
 auto* Self=N->FindPin(TEXT("self"));if(!Self||Self->LinkedTo.Num()!=1)return false;
 auto* A=Self->LinkedTo[0]->GetOwningNode()->FindPin(TEXT("self"));if(!A||A->LinkedTo.Num()!=1)return false;
 return A->LinkedTo[0]->GetOwningNode()->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Contains(TEXT("1P"));
}
static void Audit()
{
 auto* BP=LoadObject<UBlueprint>(nullptr,*(Root+TEXT("Integration/Components/BP_AssetPresentationRelay")));check(BP);
 FString Report;
 for(UEdGraph* G:BP->UbergraphPages)for(UEdGraphNode* N:G->Nodes)
 {
  auto* P=N->FindPin(TEXT("MontageToPlay"));if(!P)P=N->FindPin(TEXT("Animation"));
  if(!P||!P->DefaultObject||!P->DefaultObject->GetName().Contains(TEXT("Holster")))continue;
  Report+=N->GetName()+TEXT(" 1P=")+(Is1P(N)?TEXT("true "):TEXT("false "))+P->DefaultObject->GetPathName()+TEXT("\n");
  if(auto* M=Cast<UAnimMontage>(P->DefaultObject))for(const auto& T:M->SlotAnimTracks)for(const auto& S:T.AnimTrack.AnimSegments)
  {
   Report+=FString::Printf(TEXT(" montage=%f start=%f end=%f rate=%f seq=%s\n"),M->GetPlayLength(),S.AnimStartTime,S.AnimEndTime,S.AnimPlayRate,*GetPathNameSafe(S.GetAnimReference()));
   if(auto* A=Cast<UAnimSequence>(S.GetAnimReference())){auto* D=A->GetDataModel();TArray<FName> Names;D->GetBoneTrackNames(Names);if(Names.Num())Report+=Names[0].ToString()+TEXT(" frame0=")+D->GetBoneTrackTransform(Names[0],0).ToString()+TEXT("\n");}
  }
 }
 FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("Codex/ShortHolsterSources.txt")));
 UE_LOG(LogTemp,Display,TEXT("SHORT_TEST DrawRate %s value=%f"),FMath::IsNearlyEqual(UWeaponData::GetSwitchAnimationRate(RogueTags::Weapon_Type_Pistol),1.5f)?TEXT("PASS"):TEXT("FAIL"),UWeaponData::GetSwitchAnimationRate(RogueTags::Weapon_Type_Pistol));
}
static FAutoConsoleCommand AuditCmd(TEXT("FPS.Temp.ShortHolsterAudit"),TEXT("Read-only holster audit"),FConsoleCommandDelegate::CreateStatic(&Audit));
}
#endif
