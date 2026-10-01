#include "Animation/AnimNotify_FirearmSound.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "Weapon/WeaponBase.h"
#include "Tags/GameplayTags.h"
USkeletalMeshComponent* UAnimNotify_FirearmSound::ResolveEmitter(USkeletalMeshComponent* Source,bool bSingleView)
{
 if(!IsValid(Source)||!Source->GetWorld()||Source->GetWorld()->GetNetMode()==NM_DedicatedServer)return nullptr;
 if(auto* Weapon=Cast<AWeaponBase>(Source->GetOwner()))
 {
  if(Weapon->GetWeaponTag()==RogueTags::Weapon_Type_Knife)return Source;
  if(bSingleView)
  {
   const auto* Pawn=Cast<APawn>(Weapon->GetOwner());
   const auto* Trigger=Pawn&&Pawn->IsLocallyControlled()?Weapon->GetMesh1P():Weapon->GetMesh3P();
   return Source==Trigger?Weapon->GetMesh3P():nullptr;
  }
  return Source==Weapon->GetMesh3P()?Source:nullptr;
 }
 if(auto* Character=Cast<AShooterCharacter>(Source->GetOwner()))
 {
  auto* Weapon=Character->GetCombatComponent()?Character->GetCombatComponent()->GetCurrentWeapon():nullptr;
  if(Weapon&&Weapon->GetWeaponTag()!=RogueTags::Weapon_Type_Knife)
   return Source==Character->GetMesh()?Weapon->GetMesh3P():nullptr;
 }
 // Shared clips retain unrelated actor and animation-editor playback.
 return Source;
}
void UAnimNotify_FirearmSound::Notify(USkeletalMeshComponent* MeshComp,UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference)
{
 // Keep the source asset useful in previews, but don't replay a shot already
 // emitted by the native weapon event (the sniper clip also contains bolt audio).
 if(bGunshotHandledByWeapon&&MeshComp&&Cast<AWeaponBase>(MeshComp->GetOwner())
  &&MeshComp->GetWorld()&&MeshComp->GetWorld()->WorldType!=EWorldType::EditorPreview)return;
 if(auto* Emitter=ResolveEmitter(MeshComp,bSingleViewAction))Super::Notify(Emitter,Animation,EventReference);
}
