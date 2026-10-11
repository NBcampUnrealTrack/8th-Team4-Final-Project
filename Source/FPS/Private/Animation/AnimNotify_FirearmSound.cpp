#include "Animation/AnimNotify_FirearmSound.h"
#include "Audio/CharacterSoundPlayback.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/WeaponEffectsComponent.h"
#include "Tags/GameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Animation/AnimInstance.h"
USkeletalMeshComponent* UAnimNotify_FirearmSound::ResolveEmitter(USkeletalMeshComponent* Source,bool bSingleView)
{
 if(!IsValid(Source)||!Source->GetWorld()||Source->GetWorld()->GetNetMode()==NM_DedicatedServer)return nullptr;
 if(auto* Weapon=Cast<AWeaponBase>(Source->GetOwner()))
 {
  if(Weapon->GetWeaponTag()==RogueTags::Weapon_Type_Knife)return Source;
  // Unmigrated inspect/holster animations play on one view only. Preserve their
  // trigger, while still locating the audible emitter on the world-space gun.
  if(bSingleView)
  {
   const auto* Pawn=Cast<APawn>(Weapon->GetOwner());
   const auto* Trigger=Pawn&&Pawn->IsLocallyControlled()?Weapon->GetMesh1P():Weapon->GetMesh3P();
   return Source==Trigger?Weapon->GetMesh3P():nullptr;
  }
  // Reload/bolt assets opt out of the legacy trigger and run solely on 3P.
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
 if(!MeshComp||!MeshComp->GetWorld())return;
 if(MeshComp->GetWorld()->WorldType==EWorldType::EditorPreview)
 {
  Super::Notify(MeshComp,Animation,EventReference);
  return;
 }
 // Fire is emitted once by the native weapon event. Draw is emitted by the
 // character's 3P montage, so legacy weapon Draw tracks are preview-only.
 if(bPreviewOnly||bGunshotHandledByWeapon)return;
 auto* Emitter=ResolveEmitter(MeshComp,bSingleViewAction);
 if(!Emitter)return;
 if (!bSingleViewAction)
 {
  // Leader/follower synchronization can cross the same notify twice. Keep
  // consumption on this weapon instance; shared notify assets hold no state.
  if (auto* Weapon = Cast<AWeaponBase>(MeshComp->GetOwner()))
  {
   if (Weapon->GetWeaponTag() != RogueTags::Weapon_Type_Knife)
   {
    if (auto* Effects = Weapon->FindComponentByClass<UWeaponEffectsComponent>())
    {
     if (!Effects->ConsumeMontageSoundNotify(MeshComp->GetAnimInstance(), EventReference)) return;
    }
   }
  }
 }
 // Keep the canonical trigger and notify dedup, then choose the listener perspective.
 if(Sound && Sound->IsOneShot()
  && CharacterSoundPlayback::TryPlayLocal2D(Emitter->GetOwner(),Sound,VolumeMultiplier,PitchMultiplier))return;
 if(!AttenuationOverride)
 {
  Super::Notify(Emitter,Animation,EventReference);
  return;
 }
 if(!Sound||!Sound->IsOneShot())return;
 // An action follows the 3P weapon even while the player moves during reload.
 UGameplayStatics::SpawnSoundAttached(Sound,Emitter,AttachName,FVector::ZeroVector,
  EAttachLocation::SnapToTarget,false,VolumeMultiplier,PitchMultiplier,0.f,AttenuationOverride);
}
