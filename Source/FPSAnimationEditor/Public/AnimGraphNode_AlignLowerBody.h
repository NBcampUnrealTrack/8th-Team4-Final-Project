#pragma once
#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/AnimNode_AlignLowerBody.h"
#include "AnimGraphNode_AlignLowerBody.generated.h"
UCLASS()
class FPSANIMATIONEDITOR_API UAnimGraphNode_AlignLowerBody : public UAnimGraphNode_SkeletalControlBase
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,Category=Settings) FAnimNode_AlignLowerBody Node;
 virtual FText GetNodeTitle(ENodeTitleType::Type) const override {return FText::FromString(TEXT("Align Standing Lower Body"));}
 virtual FString GetNodeCategory() const override {return TEXT("FPS|Posture");}
protected:
 virtual const FAnimNode_SkeletalControlBase* GetNode() const override {return &Node;}
};
