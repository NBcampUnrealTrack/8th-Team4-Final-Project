#include "Training/CQB/CQBTrainingEndActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Training/CQB/CQBTrainingSpawner.h"
#include "Training/TrainingPlayerComponent.h"
#include "../TrainingInteractionRegistration.h"

ACQBTrainingEndActor::ACQBTrainingEndActor()
{
    PrimaryActorTick.bCanEverTick = false;

    InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
    SetRootComponent(InteractionCollision);
    TrainingInteractionRegistration::ConfigureCollision(*InteractionCollision, FVector(45.0, 45.0, 60.0));
    InteractionCollision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleInteractionBeginOverlap);
    InteractionCollision->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleInteractionEndOverlap);

    DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
    TrainingInteractionRegistration::ConfigureDisplayMesh(*DisplayMesh, *InteractionCollision);
}

void ACQBTrainingEndActor::BeginPlay()
{
    Super::BeginPlay();

    RefreshEnabledState();
}

void ACQBTrainingEndActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    TrainingInteractionRegistration::Cleanup(*this, LocalOverlappingController);
    Super::EndPlay(EndPlayReason);
}

bool ACQBTrainingEndActor::CanInteract(const APlayerController& RequestingPlayer) const
{
    return bEndEnabled
        && IsValid(Spawner)
        && Spawner->IsTrainingComplete()
        && RequestingPlayer.GetPawn() != nullptr
        && RequestingPlayer.FindComponentByClass<UTrainingPlayerComponent>() != nullptr;
}

void ACQBTrainingEndActor::Interact(APlayerController& RequestingPlayer)
{
    if (!CanInteract(RequestingPlayer))
    {
        return;
    }

    if (UTrainingPlayerComponent* Training = RequestingPlayer.FindComponentByClass<UTrainingPlayerComponent>())
    {
        Training->OpenCQBEndDialog(*this);
    }
}

void ACQBTrainingEndActor::SetEndEnabled(const bool bEnabled)
{
    if (!bEnabled)
    {
        TrainingInteractionRegistration::Cleanup(*this, LocalOverlappingController);
    }
    bEndEnabled = bEnabled;
    RefreshEnabledState();
}

void ACQBTrainingEndActor::RefreshEnabledState()
{
    if (InteractionCollision)
    {
        InteractionCollision->SetCollisionEnabled(bEndEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
        InteractionCollision->SetGenerateOverlapEvents(bEndEnabled);
    }
    if (DisplayMesh)
    {
        DisplayMesh->SetVisibility(bEndEnabled, true);
        DisplayMesh->SetHiddenInGame(!bEndEnabled, true);
    }
}

void ACQBTrainingEndActor::HandleInteractionBeginOverlap(
    UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (bEndEnabled)
    {
        TrainingInteractionRegistration::Register(*this, OtherActor, LocalOverlappingController);
    }
}

void ACQBTrainingEndActor::HandleInteractionEndOverlap(
    UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
    TrainingInteractionRegistration::Unregister(*this, OtherActor, LocalOverlappingController);
}
