#include "Training/TrainingInteractionActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Training/TrainingArena.h"
#include "Training/TrainingPlayerComponent.h"

ATrainingInteractionActor::ATrainingInteractionActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
    SetRootComponent(InteractionCollision);
    InteractionCollision->SetBoxExtent(FVector(35.0, 35.0, 50.0));
    InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionCollision->SetCollisionObjectType(ECC_WorldDynamic);
    InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    InteractionCollision->SetCanEverAffectNavigation(false);
    DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
    DisplayMesh->SetupAttachment(InteractionCollision);
    DisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DisplayMesh->SetCanEverAffectNavigation(false);
}

bool ATrainingInteractionActor::CanInteract(const APlayerController& RequestingPlayer) const
{
    return IsValid(Arena) && RequestingPlayer.GetPawn()
        && RequestingPlayer.FindComponentByClass<UTrainingPlayerComponent>() != nullptr;
}

void ATrainingInteractionActor::Interact(APlayerController& RequestingPlayer)
{
    if (HasAuthority() && CanInteract(RequestingPlayer))
    {
        RequestingPlayer.FindComponentByClass<UTrainingPlayerComponent>()->OpenDialogFor(*this);
    }
}
