#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "AppearancePoseAnimInstance.generated.h"

class USkeletalMeshComponent;
UCLASS(Transient)
class FPS_API UAppearancePoseAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> PoseSource;
    UPROPERTY(Transient) bool bPreserveReferencePose = true;
    UPROPERTY(Transient) bool bPreserveWeaponHandRelationship = false;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
