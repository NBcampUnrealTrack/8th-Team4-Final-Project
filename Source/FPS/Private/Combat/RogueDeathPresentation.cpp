#include "Combat/RogueDeathPresentation.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Damage/RogueDamageTypes.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void RogueDeathPresentation::StartRagdoll(ACharacter& Character, const FRogueDeathReaction& Reaction,
    const FRogueRagdollSettings& Settings)
{
    // Extracted from ShooterCharacter: preserve its collision, velocity and dedicated-server behavior.
    const FVector DeathVelocity = Character.GetVelocity();
    Character.GetCharacterMovement()->DisableMovement();
    Character.GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    USkeletalMeshComponent* BodyMesh = Character.GetMesh();
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    BodyMesh->SetCollisionObjectType(ECC_PhysicsBody);
    BodyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    BodyMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    BodyMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

    if (!BodyMesh->GetPhysicsAsset())
    {
        UE_LOG(LogTemp, Warning, TEXT("[RogueDeath] %s has no PhysicsAsset; ragdoll cannot start."),
            *GetNameSafe(&Character));
        return;
    }
    if (Character.GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    BodyMesh->SetAllBodiesSimulatePhysics(true);
    BodyMesh->SetSimulatePhysics(true);
    BodyMesh->SetAllBodiesPhysicsBlendWeight(1.0f);
    BodyMesh->WakeAllRigidBodies();
    BodyMesh->SetAllPhysicsLinearVelocity(DeathVelocity);

    if (Reaction.Direction.IsNearlyZero() || Settings.ImpulseSpeed <= 0.0f)
    {
        return;
    }
    const FName FallbackBone = Reaction.bHeadshot ? Settings.HeadshotImpulseBone : Settings.ImpulseBone;
    FName TargetBone = NAME_None;
    for (const FName CandidateBone : { Reaction.HitBoneName, FallbackBone })
    {
        if (!CandidateBone.IsNone() && BodyMesh->GetBodyInstance(CandidateBone))
        {
            TargetBone = CandidateBone;
            break;
        }
    }
    FVector ImpulseDirection = Reaction.Direction;
    ImpulseDirection.Z += Settings.UpwardBias;
    if (ImpulseDirection.Normalize())
    {
        const float ImpulseSpeed = Settings.ImpulseSpeed
            * (Reaction.bHeadshot ? Settings.HeadshotImpulseMultiplier : 1.0f);
        BodyMesh->AddImpulse(ImpulseDirection * ImpulseSpeed, TargetBone, true);
    }
}
