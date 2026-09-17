#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Animation/AnimData/IAnimationDataModel.h"
namespace InfimaReloadAudit {
static void Run(){
 TArray<FString> Lines;
 auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
 TArray<FAssetData> Assets;Registry.GetAssetsByPath(TEXT("/Game/MetaHumans/Anim/FirstAnim/Weapons/Pistol"),Assets,true);Registry.GetAssetsByPath(TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/WK-11_Viper"),Assets,true);
 for(auto& A:Assets)if(auto* S=Cast<UAnimSequenceBase>(A.GetAsset())){
  Lines.Add(FString::Printf(TEXT("ANIM %s length=%.3f notifies=%d"),*S->GetPathName(),S->GetPlayLength(),S->Notifies.Num()));
  for(auto& N:S->Notifies){FString Desc; if(N.Notify)for(TFieldIterator<FProperty>P(N.Notify->GetClass());P;++P){FString V;P->ExportText_InContainer(0,V,N.Notify,N.Notify,N.Notify,PPF_None);Desc+=P->GetName()+TEXT("=")+V+TEXT(";");}Lines.Add(FString::Printf(TEXT(" NOTIFY %.3f %s %s"),N.GetTime(),*GetPathNameSafe(N.Notify),*Desc));}
 }
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/MetaHumans/Anim/FirstAnim/Weapons/Shotgun/Infima/Rig/SK_InfimaShotgun"));
 TMap<int,int> Counts;for(auto& Sec:Mesh->GetImportedModel()->LODModels[0].Sections)for(auto& V:Sec.SoftVertices)for(int I=0;I<MAX_TOTAL_INFLUENCES;++I)if(V.InfluenceWeights[I])Counts.FindOrAdd(Sec.BoneMap[V.InfluenceBones[I]])++;
 for(auto& P:Counts)Lines.Add(FString::Printf(TEXT("WEIGHT %s %d"),*Mesh->GetRefSkeleton().GetBoneName(P.Key).ToString(),P.Value));
 auto* Mag=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/InfimaGames/ModernGunsBundle/ModernShotgun/Meshes/SM_Shotgun_Magazine"));Lines.Add(TEXT("MAG_BOUNDS ")+Mag->GetBounds().ToString());
 for(const TCHAR* Kind:{TEXT("Tac"),TEXT("Empty")}){auto* Seq=LoadObject<UAnimSequence>(nullptr,*(FString(TEXT("/Game/MetaHumans/Anim/FirstAnim/Weapons/Shotgun/Infima/WeaponAnimations/A_InfimaShotgun_Reload_"))+Kind));auto* M=Seq->GetDataModel();for(double T=0;T<Seq->GetPlayLength();T+=.15){auto X=M->EvaluateBoneTrackTransform(TEXT("Magazine"),M->GetFrameRate().AsFrameTime(T),EAnimInterpolationType::Linear);Lines.Add(FString::Printf(TEXT("MAG %s %.3f %s"),Kind,T,*X.ToString()));}}
 FFileHelper::SaveStringArrayToFile(Lines,*(FPaths::ProjectSavedDir()+TEXT("Codex/ReloadSoundAudit.txt")));
}
static FAutoConsoleCommand Cmd(TEXT("FPS.Infima.ReloadAudit"),TEXT("Read only asset audit"),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
