#pragma once
#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/AnimNode_ReloadHandContact.h"
#include "AnimGraphNode_ReloadHandContact.generated.h"
UCLASS()
class FPSANIMATIONEDITOR_API UAnimGraphNode_ReloadHandContact : public UAnimGraphNode_SkeletalControlBase
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,Category=Settings) FAnimNode_ReloadHandContact Node;
 virtual FText GetNodeTitle(ENodeTitleType::Type) const override {return FText::FromString(TEXT("Reload Magazine Hand Contact"));}
 virtual FString GetNodeCategory() const override {return TEXT("FPS|Posture");}
protected:
 virtual const FAnimNode_SkeletalControlBase* GetNode() const override {return &Node;}
};
