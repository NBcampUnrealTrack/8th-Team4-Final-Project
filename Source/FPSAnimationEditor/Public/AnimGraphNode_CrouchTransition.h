#pragma once
#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/AnimNode_CrouchTransition.h"
#include "AnimGraphNode_CrouchTransition.generated.h"
UCLASS()
class FPSANIMATIONEDITOR_API UAnimGraphNode_CrouchTransition : public UAnimGraphNode_SkeletalControlBase
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,Category=Settings) FAnimNode_CrouchTransition Node;
 virtual FText GetNodeTitle(ENodeTitleType::Type) const override {return FText::FromString(TEXT("Crouch / Stand Transition → Locomotion"));}
 virtual FString GetNodeCategory() const override {return TEXT("FPS|Posture");}
protected:
 virtual const FAnimNode_SkeletalControlBase* GetNode() const override {return &Node;}
};
