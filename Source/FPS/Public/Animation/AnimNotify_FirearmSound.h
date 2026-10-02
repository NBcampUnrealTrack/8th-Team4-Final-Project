#pragma once
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "AnimNotify_FirearmSound.generated.h"
UCLASS()
class FPS_API UAnimNotify_FirearmSound : public UAnimNotify_PlaySound
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,Category="Audio Routing") bool bSingleViewAction=false;
 UPROPERTY(EditAnywhere,Category="Audio Routing") bool bGunshotHandledByWeapon=false;
 static USkeletalMeshComponent* ResolveEmitter(USkeletalMeshComponent* Source,bool bSingleView=false);
 virtual void Notify(USkeletalMeshComponent* MeshComp,UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference) override;
};
