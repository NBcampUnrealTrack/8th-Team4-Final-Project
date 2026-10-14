#pragma once
#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/AnimNode_GroundFeet.h"
#include "AnimGraphNode_GroundFeet.generated.h"

UCLASS()
class FPSANIMATIONEDITOR_API UAnimGraphNode_GroundFeet : public UAnimGraphNode_SkeletalControlBase
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,Category=Settings) FAnimNode_GroundFeet Node;
 virtual FText GetNodeTitle(ENodeTitleType::Type) const override {return FText::FromString(TEXT("FPS Shared Foot Grounding"));}
 virtual FText GetTooltipText() const override {return FText::FromString(TEXT("Grounded-only terrain IK. Preserves airborne animation and limb length."));}
 virtual FString GetNodeCategory() const override {return TEXT("FPS|Grounding");}
protected:
 virtual const FAnimNode_SkeletalControlBase* GetNode() const override {return &Node;}
};
