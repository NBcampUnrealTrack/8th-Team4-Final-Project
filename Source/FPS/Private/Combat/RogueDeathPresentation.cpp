#include "Combat/RogueDeathPresentation.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/Damage/RogueDamageTypes.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GroomComponent.h"
#include "PhysicsEngine/ConstraintInstance.h"

void RogueDeathPresentation::RefreshRagdollFollowers(ACharacter& Character)
{
    auto* Body = Character.GetMesh();
    if (!Body || !Body->IsSimulatingPhysics()) return;
    TInlineComponentArray<USkeletalMeshComponent*> Meshes(&Character);
    for (auto* Mesh : Meshes)
    {
        if (Mesh == Body || !Mesh->IsAttachedTo(Body)) continue;
        // Copy-pose followers must see the final physics pose, not the pre-physics animation.
        Mesh->SetTickGroup(TG_PostPhysics);
        Mesh->AddTickPrerequisiteComponent(Body);
        Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        // Copy-pose clothing without a PhysicsAsset only has reference-pose bounds.
        // During ragdoll the component origin can move below the floor while its
        // skinned bones remain above it, so those bounds incorrectly cull the outfit.
        // Leader-pose meshes already derive bounds from their leader's physics asset.
        if (!Mesh->LeaderPoseComponent.IsValid() && !Mesh->GetPhysicsAsset())
        {
            Mesh->bUseAttachParentBound = true;
            Mesh->UpdateBounds();
        }
    }
    TInlineComponentArray<UGroomComponent*> Grooms(&Character);
    for (auto* Groom : Grooms)
    {
        auto* Parent = Cast<USkeletalMeshComponent>(Groom->GetAttachParent());
        if (!Parent || (Parent != Body && !Parent->IsAttachedTo(Body)) || Parent->GetBoneIndex(TEXT("head")) == INDEX_NONE) continue;
        // Secondary strand simulation can collapse through a ragdolled scalp.
        // Keep corpse hair skinned to the final pose; living grooms retain simulation.
        if (!Groom->SimulationSettings.bOverrideSettings || Groom->SimulationSettings.SolverSettings.bEnableSimulation)
        {
            Groom->SimulationSettings.bOverrideSettings = true;
            // UE only rebuilds simulation resources when the stored override changes.
            // Its default is already false even while asset-driven simulation is on.
            Groom->SimulationSettings.SolverSettings.bEnableSimulation = true;
            Groom->SetEnableSimulation(false);
        }
        // Rigid groom LODs use AttachmentName even with a binding asset. Facial-only
        // bones are not present in the body leader pose; head exists in both rigs.
        // GroomComponent applies the inverse reference-bone transform on its next tick.
        Groom->AttachmentName = TEXT("head");
        Groom->SetTickGroup(TG_PostPhysics);
        Groom->AddTickPrerequisiteComponent(Parent);
        Groom->AddTickPrerequisiteComponent(Body);
        Groom->ResetSimulation();
    }
}

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
    // Corpse joints keep their limits and damping, but must not hold the
    // animated weapon pose against gravity. Change instances, not the asset.
    for (FConstraintInstance* Constraint : BodyMesh->Constraints)
    {
        if (!Constraint) continue;
        Constraint->SetOrientationDriveTwistAndSwing(false, false);
        Constraint->SetOrientationDriveSLERP(false);
    }
    BodyMesh->WakeAllRigidBodies();
    BodyMesh->SetAllPhysicsLinearVelocity(DeathVelocity);
    RefreshRagdollFollowers(Character);

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
