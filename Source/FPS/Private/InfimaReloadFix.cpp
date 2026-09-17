#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/StaticMesh.h"
#include "Weapon/WeaponBase.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "HAL/IConsoleManager.h"
namespace InfimaReloadFix {
const FString Root=TEXT("/Game/MetaHumans/Anim/FirstAnim/Weapons/");
template<class T>T* Load(const FString& P){auto* O=LoadObject<T>(nullptr,*P);check(O);return O;}
static void Save(UObject* O){O->MarkPackageDirty();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;check(UPackage::SavePackage(O->GetOutermost(),O,*FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),A));}
static USCS_Node* MagazineNode(UBlueprint* BP){for(auto* N:BP->SimpleConstructionScript->GetAllNodes())if(N->GetVariableName()==TEXT("InfimaMagazine1P"))return N;return nullptr;}
static void Test(){
 int Fail=0;
 for(const TCHAR* Name:{TEXT("Draw"),TEXT("Holster"),TEXT("Mag_Check"),TEXT("Reload_Tac"),TEXT("Reload_Empty")}){
  auto* A=Load<UAnimSequence>(Root+TEXT("Pistol/Infima/WeaponAnimations/A_W_WK-11_Viper_")+Name);int Sounds=0;for(auto& N:A->Notifies)if(Cast<UAnimNotify_PlaySound>(N.Notify))++Sounds;
  if(Sounds!=1){++Fail;UE_LOG(LogTemp,Display,TEXT("RELOAD_TEST_FAIL pistol_missing_or_duplicate_sound %s count=%d"),Name,Sounds);}
 }
 auto* Inspect=Load<UAnimMontage>(Root+TEXT("Pistol/Infima/Montages/AM_W_WK-11_Viper_Inspect"));if(Inspect->Notifies.Num()!=1){++Fail;UE_LOG(LogTemp,Display,TEXT("RELOAD_TEST_FAIL inspect_sound"));}
 auto* BP=Load<UBlueprint>(TEXT("/Game/FPS/Weapon/BP/BP_Weapon_Shotgun"));auto* N=MagazineNode(BP);auto* C=N?Cast<UStaticMeshComponent>(N->ComponentTemplate):nullptr;
 if(!C||!C->GetStaticMesh()||C->GetStaticMesh()->GetName()!=TEXT("SM_Shotgun_Magazine")||N->AttachToName!=TEXT("Magazine")){++Fail;UE_LOG(LogTemp,Display,TEXT("RELOAD_TEST_FAIL missing_actual_magazine"));}
 for(const TCHAR* Kind:{TEXT("Tac"),TEXT("Empty")})for(const TCHAR* Side:{TEXT("FP"),TEXT("W")}){
  auto* M=Load<UAnimMontage>(Root+TEXT("Shotgun/Infima/Montages/AM_")+Side+TEXT("_InfimaShotgun_Reload_")+Kind);auto& Segs=M->SlotAnimTracks[0].AnimTrack.AnimSegments;
  double Insertion=0;for(auto& S:Segs){double Overlap=FMath::Max(0.,FMath::Min((double)S.AnimEndTime,2.1)-FMath::Max((double)S.AnimStartTime,1.25));Insertion+=Overlap/S.AnimPlayRate*2.8/M->GetPlayLength();}
  if(Insertion<.9){++Fail;UE_LOG(LogTemp,Display,TEXT("RELOAD_TEST_FAIL rushed_insertion %s %s seconds=%.3f"),Kind,Side,Insertion);}
 }
 UE_LOG(LogTemp,Display,TEXT("RELOAD_TEST_RESULT failures=%d"),Fail);
}
static void Apply(){
 // Copy only audio notifies: importing the asset's ReloadWeapon notify would change gameplay ammunition.
 for(const TCHAR* Name:{TEXT("Draw"),TEXT("Holster"),TEXT("Mag_Check"),TEXT("Reload_Tac"),TEXT("Reload_Empty"),TEXT("Inspect")}){
  bool Inspect=FString(Name)==TEXT("Inspect");FString Prefix=Inspect?TEXT("AM_W_WK-11_Viper_"):TEXT("A_W_WK-11_Viper_");
  auto* S=Load<UAnimSequenceBase>(TEXT("/Game/KINEMATION/TacticalShooterPack/Animations/UE5/WK-11_Viper/Weapon/")+Prefix+Name);
  auto* D=Load<UAnimSequenceBase>(Root+TEXT("Pistol/Infima/")+(Inspect?TEXT("Montages/"):TEXT("WeaponAnimations/"))+Prefix+Name);
  D->Notifies.RemoveAll([](const FAnimNotifyEvent& E){return Cast<UAnimNotify_PlaySound>(E.Notify)!=nullptr;});
  for(auto& E:S->Notifies)if(auto* Sound=Cast<UAnimNotify_PlaySound>(E.Notify)){FAnimNotifyEvent N=E;auto* Copy=CastChecked<UAnimNotify_PlaySound>(StaticDuplicateObject(Sound,D,NAME_None));
   if(FString(Name).StartsWith(TEXT("Reload")))Copy->PitchMultiplier*=D->GetPlayLength()/1.4f;
   N.Notify=Copy;N.NotifyStateClass=nullptr;N.Link(D,E.GetTime());N.Guid=FGuid::NewGuid();D->Notifies.Add(N);
  }Save(D);
 }
 auto* BP=Load<UBlueprint>(TEXT("/Game/FPS/Weapon/BP/BP_Weapon_Shotgun"));auto* N=MagazineNode(BP);if(!N){N=BP->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(),TEXT("InfimaMagazine1P"));BP->SimpleConstructionScript->AddNode(N);}
 auto* C=CastChecked<UStaticMeshComponent>(N->ComponentTemplate);C->SetStaticMesh(Load<UStaticMesh>(TEXT("/Game/InfimaGames/ModernGunsBundle/ModernShotgun/Meshes/SM_Shotgun_Magazine")));C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetOnlyOwnerSee(true);C->CastShadow=false;C->SetRelativeTransform(FTransform::Identity);
 N->SetParent(CastChecked<AWeaponBase>(BP->GeneratedClass->GetDefaultObject())->GetMesh1P());N->AttachToName=TEXT("Magazine");
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);Save(BP);
 // Preserve the montage duration used by Relay. Split both actors at identical source times.
 for(const TCHAR* Kind:{TEXT("Tac"),TEXT("Empty")})for(const TCHAR* Side:{TEXT("FP"),TEXT("W")}){
  auto* M=Load<UAnimMontage>(Root+TEXT("Shotgun/Infima/Montages/AM_")+Side+TEXT("_InfimaShotgun_Reload_")+Kind);auto& Track=M->SlotAnimTracks[0].AnimTrack;auto Base=Track.AnimSegments[0];UAnimSequenceBase* Seq=Base.GetAnimReference();double L=Seq->GetPlayLength();
  const double Source[]={0,.8,1.25,2.1,L};const bool Empty=FString(Kind)==TEXT("Empty");const double Wall[]={Empty?.4:.45,Empty?.35:.4,Empty?1.:.95,1.05-(Empty?0.:.05)};
  Track.AnimSegments.Reset();double Position=0;for(int I=0;I<4;++I){auto S=Base;S.StartPos=Position;S.AnimStartTime=Source[I];S.AnimEndTime=Source[I+1];S.AnimPlayRate=(Source[I+1]-Source[I])/(Wall[I]*L/2.8);S.LoopingCount=1;Track.AnimSegments.Add(S);Position+=Wall[I]*L/2.8;}check(FMath::IsNearlyEqual(Position,L,.001));Save(M);
 }
 Test();
}
static FAutoConsoleCommand TestCmd(TEXT("FPS.Infima.ReloadTest"),TEXT("Asset regression checks"),FConsoleCommandDelegate::CreateStatic(&Test));
static FAutoConsoleCommand FixCmd(TEXT("FPS.Infima.ReloadFix"),TEXT("Scoped asset repair"),FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
