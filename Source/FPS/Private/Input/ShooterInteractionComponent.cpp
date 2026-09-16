#include "Input/ShooterInteractionComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/RogueInteractable.h"
#include "Player/ShooterPlayerController.h"

UShooterInteractionComponent::UShooterInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

AActor* UShooterInteractionComponent::FindInteractableInView() const
{
    const AShooterPlayerController* Player = Cast<AShooterPlayerController>(GetOwner());
    if (!Player || !Player->GetPawn() || !Player->CanProcessActionInput())
    {
        return nullptr;
    }
    FVector ViewOrigin;
    FRotator ViewRotation;
    Player->GetPlayerViewPoint(ViewOrigin, ViewRotation);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShooterInteraction), false, Player->GetPawn());
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, ViewOrigin,
        ViewOrigin + ViewRotation.Vector() * InteractionDistanceCentimeters,
        InteractionTraceChannel, QueryParams))
    {
        return nullptr;
    }
    AActor* Candidate = Hit.GetActor();
    const IRogueInteractable* Interactable = Cast<IRogueInteractable>(Candidate);
    return Interactable && Interactable->CanInteract(*Player) ? Candidate : nullptr;
}

bool UShooterInteractionComponent::TryInteract()
{
    AActor* Candidate = FindInteractableInView();
    if (!Candidate)
    {
        return false;
    }
    ServerInteract(Candidate);
    return true;
}

bool UShooterInteractionComponent::CanReachInteractable(const AActor* Candidate) const
{
    const AShooterPlayerController* Player = Cast<AShooterPlayerController>(GetOwner());
    const IRogueInteractable* Interactable = Cast<IRogueInteractable>(Candidate);
    if (!Player || !Player->GetPawn() || !IsValid(Candidate) || !Interactable
        || Candidate->GetWorld() != GetWorld() || !Player->CanProcessActionInput()
        || !Interactable->CanInteract(*Player))
    {
        return false;
    }

    FVector ViewOrigin;
    FRotator ViewRotation;
    Player->GetPlayerViewPoint(ViewOrigin, ViewRotation);
    const FVector Destination = Candidate->GetActorLocation();
    if (FVector::DistSquared(ViewOrigin, Destination) > FMath::Square(InteractionDistanceCentimeters))
    {
        return false;
    }
    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShooterInteractionConfirm), false, Player->GetPawn());
    const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
        Hit, ViewOrigin, Destination, InteractionTraceChannel, QueryParams);
    return !bBlocked || Hit.GetActor() == Candidate;
}

void UShooterInteractionComponent::ServerInteract_Implementation(AActor* Candidate)
{
    // Recheck authority-side reach and occlusion, rather than trusting the client's hit result.
    if (CanReachInteractable(Candidate))
    {
        IRogueInteractable* Interactable = Cast<IRogueInteractable>(Candidate);
        Interactable->Interact(*CastChecked<APlayerController>(GetOwner()));
    }
}
