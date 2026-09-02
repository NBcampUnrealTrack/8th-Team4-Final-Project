#include "Animation/AnimNotify_Footstep.h"

#include "Character/FootstepComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_Footstep::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	UFootstepComponent* FootstepComponent = IsValid(Owner)
		? Owner->FindComponentByClass<UFootstepComponent>()
		: nullptr;
	if (IsValid(FootstepComponent))
	{
		FootstepComponent->PlayFootstep(MeshComp, FootSocketName);
	}
}

FString UAnimNotify_Footstep::GetNotifyName_Implementation() const
{
	return TEXT("Footstep");
}
