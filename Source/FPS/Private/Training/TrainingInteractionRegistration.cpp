#include "TrainingInteractionRegistration.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Input/ShooterInteractionComponent.h"

namespace
{
APlayerController* ResolveLocalController(AActor* OtherActor)
{
    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn || !Pawn->IsLocallyControlled())
    {
        return nullptr;
    }

    APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController());
    return PlayerController && PlayerController->IsLocalController() ? PlayerController : nullptr;
}

UShooterInteractionComponent* ResolveInteractionComponent(APlayerController* PlayerController)
{
    return PlayerController
        ? PlayerController->FindComponentByClass<UShooterInteractionComponent>()
        : nullptr;
}
}


void TrainingInteractionRegistration::ConfigureCollision(UBoxComponent& Collision, const FVector& DefaultExtent)
{
    Collision.SetBoxExtent(DefaultExtent);
    Collision.SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Collision.SetCollisionObjectType(ECC_WorldDynamic);
    Collision.SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision.SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Collision.SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Collision.SetGenerateOverlapEvents(true);
    Collision.SetCanEverAffectNavigation(false);
}

void TrainingInteractionRegistration::ConfigureDisplayMesh(UStaticMeshComponent& DisplayMesh, UBoxComponent& Parent)
{
    DisplayMesh.SetupAttachment(&Parent);
    DisplayMesh.SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DisplayMesh.SetCanEverAffectNavigation(false);
}

void TrainingInteractionRegistration::Register(AActor& Interactable, AActor* OtherActor,
    TWeakObjectPtr<APlayerController>& InOutLocalController)
{
    APlayerController* PlayerController = ResolveLocalController(OtherActor);
    UShooterInteractionComponent* Interaction = ResolveInteractionComponent(PlayerController);
    if (!PlayerController || !Interaction)
    {
        return;
    }

    if (APlayerController* PreviousController = InOutLocalController.Get();
        PreviousController && PreviousController != PlayerController)
    {
        if (UShooterInteractionComponent* PreviousInteraction = ResolveInteractionComponent(PreviousController))
        {
            PreviousInteraction->UnregisterProximityInteractable(&Interactable);
        }
    }

    InOutLocalController = PlayerController;
    Interaction->RegisterProximityInteractable(&Interactable);
}

void TrainingInteractionRegistration::Unregister(AActor& Interactable, AActor* OtherActor,
    TWeakObjectPtr<APlayerController>& InOutLocalController)
{
    APlayerController* PlayerController = ResolveLocalController(OtherActor);
    if (!PlayerController || PlayerController != InOutLocalController.Get())
    {
        return;
    }

    if (UShooterInteractionComponent* Interaction = ResolveInteractionComponent(PlayerController))
    {
        Interaction->UnregisterProximityInteractable(&Interactable);
    }
    InOutLocalController.Reset();
}

void TrainingInteractionRegistration::Cleanup(AActor& Interactable,
    TWeakObjectPtr<APlayerController>& InOutLocalController)
{
    if (APlayerController* PlayerController = InOutLocalController.Get())
    {
        if (UShooterInteractionComponent* Interaction = ResolveInteractionComponent(PlayerController))
        {
            Interaction->UnregisterProximityInteractable(&Interactable);
        }
    }
    InOutLocalController.Reset();
}
