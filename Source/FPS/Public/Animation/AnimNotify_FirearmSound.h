#pragma once

#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "AnimNotify_FirearmSound.generated.h"

class USoundAttenuation;

UCLASS()
class FPS_API UAnimNotify_FirearmSound : public UAnimNotify_PlaySound
{
 GENERATED_BODY()

public:
 // Legacy inspect/holster trigger routing; Draw/reload/bolt do not use this.
 UPROPERTY(EditAnywhere, Category="Audio Routing")
 bool bSingleViewAction = false;

 UPROPERTY(EditAnywhere, Category="Audio Routing")
 bool bGunshotHandledByWeapon = false;

 // Source weapon Draw clips remain audible in the asset editor only.
 UPROPERTY(EditAnywhere, Category="Audio Routing")
 bool bPreviewOnly = false;

 UPROPERTY(EditAnywhere, Category="Audio Routing")
 TObjectPtr<USoundAttenuation> AttenuationOverride;

 static USkeletalMeshComponent* ResolveEmitter(USkeletalMeshComponent* Source, bool bSingleView = false);
 virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
