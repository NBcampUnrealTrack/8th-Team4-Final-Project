#include "Training/TrainingInteractionActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TrainingInteractionRegistration.h"
#include "Player/TrainingPlayerController.h"
#include "Training/TrainingArena.h"
#include "Training/CQB/CQBTrainingPlayerSpawnPoint.h"
#include "Training/TrainingPlayerComponent.h"

ATrainingInteractionActor::ATrainingInteractionActor()
{
    PrimaryActorTick.bCanEverTick = false;

    InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
    SetRootComponent(InteractionCollision);
    TrainingInteractionRegistration::ConfigureCollision(*InteractionCollision, FVector(35.0, 35.0, 50.0));
    InteractionCollision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleInteractionBeginOverlap);
    InteractionCollision->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleInteractionEndOverlap);

    DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
    TrainingInteractionRegistration::ConfigureDisplayMesh(*DisplayMesh, *InteractionCollision);
}

void ATrainingInteractionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    TrainingInteractionRegistration::Cleanup(*this, LocalOverlappingController);
    Super::EndPlay(EndPlayReason);
}

bool ATrainingInteractionActor::CanInteract(const APlayerController& RequestingPlayer) const
{
    const bool bHasDestination = InteractionRole == ETrainingInteractionRole::Entrance
        ? (IsValid(Arena) || IsValid(CQBEntryPoint))
        : IsValid(Arena);

    return GetNetMode() == NM_Standalone && RequestingPlayer.IsLocalController()
        && RequestingPlayer.IsA<ATrainingPlayerController>()
        && bHasDestination && RequestingPlayer.GetPawn()
        && RequestingPlayer.FindComponentByClass<UTrainingPlayerComponent>() != nullptr;
}

void ATrainingInteractionActor::Interact(APlayerController& RequestingPlayer)
{
    if (CanInteract(RequestingPlayer))
    {
        RequestingPlayer.FindComponentByClass<UTrainingPlayerComponent>()->OpenDialogFor(*this);
    }
}

void ATrainingInteractionActor::HandleInteractionBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    TrainingInteractionRegistration::Register(*this, OtherActor, LocalOverlappingController);
}

void ATrainingInteractionActor::HandleInteractionEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32)
{
    TrainingInteractionRegistration::Unregister(*this, OtherActor, LocalOverlappingController);
}
