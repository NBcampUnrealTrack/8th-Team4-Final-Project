#include "Training/CQB/CQBTrainingAIComponent.h"
#include "Training/CQB/CQBTrainingSpawner.h"

#include "Animation/AnimInstance.h"
#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GAS/RogueHealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Player/ShooterPlayerState.h"
#include "Weapon/WeaponBase.h"

namespace
{
    void SetWeaponRuntimeTickEnabled(AWeaponBase* Weapon, const bool bEnabled, const bool bIncludeComponents)
    {
        if (!IsValid(Weapon))
        {
            return;
        }

        Weapon->SetActorTickEnabled(bEnabled);
        if (!bIncludeComponents)
        {
            return;
        }

        TInlineComponentArray<UActorComponent*> WeaponComponents;
        Weapon->GetComponents(WeaponComponents);
        for (UActorComponent* Component : WeaponComponents)
        {
            if (IsValid(Component))
            {
                Component->SetComponentTickEnabled(bEnabled);
            }
        }
    }
}

UCQBTrainingAIComponent::UCQBTrainingAIComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

ACharacter* UCQBTrainingAIComponent::GetOwnerCharacter() const
{
    return Cast<ACharacter>(GetOwner());
}

void UCQBTrainingAIComponent::SetTrainingSpawner(ACQBTrainingSpawner* InSpawner)
{
    TrainingSpawner = InSpawner;
}

ACQBTrainingSpawner* UCQBTrainingAIComponent::GetTrainingSpawner() const
{
    return TrainingSpawner.Get();
}

void UCQBTrainingAIComponent::ConfigurePatrolRoute(
    const FVector& SpawnOrigin,
    const TArray<FVector>& InPatrolLocations,
    const bool bPingPong)
{
    PatrolRoute.SpawnOrigin = SpawnOrigin;
    PatrolRoute.bHasSpawnOrigin = true;
    PatrolRoute.bPingPong = bPingPong;
    PatrolRoute.PatrolLocations = InPatrolLocations;
}

bool UCQBTrainingAIComponent::HasValidPatrolRoute() const
{
    return PatrolRoute.bHasSpawnOrigin && !PatrolRoute.PatrolLocations.IsEmpty();
}

bool UCQBTrainingAIComponent::IsAlive() const
{
    const ACharacter* Character = GetOwnerCharacter();
    if (!Character || bDeathCleanupStarted)
    {
        return false;
    }

    const AShooterPlayerState* PlayerState = Character->GetPlayerState<AShooterPlayerState>();
    const URogueHealthComponent* Health = PlayerState ? PlayerState->GetHealthComponent() : nullptr;
    return Health && !Health->IsDead();
}

UCharacterMovementComponent* UCQBTrainingAIComponent::GetCharacterMovement() const
{
    ACharacter* Character = GetOwnerCharacter();
    return Character ? Character->GetCharacterMovement() : nullptr;
}

USkeletalMeshComponent* UCQBTrainingAIComponent::GetMesh() const
{
    ACharacter* Character = GetOwnerCharacter();
    return Character ? Character->GetMesh() : nullptr;
}

FVector UCQBTrainingAIComponent::GetActorLocation() const
{
    const AActor* Owner = GetOwner();
    return Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
}

FVector UCQBTrainingAIComponent::GetVelocity() const
{
    const AActor* Owner = GetOwner();
    return Owner ? Owner->GetVelocity() : FVector::ZeroVector;
}

FVector UCQBTrainingAIComponent::GetPawnViewLocation() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    return Pawn ? Pawn->GetPawnViewLocation() : GetActorLocation();
}

bool UCQBTrainingAIComponent::ResolveMuzzleTransform(FTransform& OutTransform) const
{
    const AShooterCharacter* Shooter = Cast<AShooterCharacter>(GetOwner());
    if (Shooter)
    {
        if (AWeaponBase* Weapon = Shooter->GetCurrentWeapon_Implementation())
        {
            if (USkeletalMeshComponent* WeaponMesh = Weapon->GetMesh3P())
            {
                if (WeaponMesh->DoesSocketExist(CombatSettings.MuzzleSocketName))
                {
                    OutTransform = WeaponMesh->GetSocketTransform(CombatSettings.MuzzleSocketName, RTS_World);
                    return true;
                }
            }
        }
    }

    if (USkeletalMeshComponent* CharacterMesh = GetMesh())
    {
        if (CharacterMesh->DoesSocketExist(CombatSettings.MuzzleSocketName))
        {
            OutTransform = CharacterMesh->GetSocketTransform(CombatSettings.MuzzleSocketName, RTS_World);
            return true;
        }
    }
    return false;
}

void UCQBTrainingAIComponent::PlayShotPresentation(
    const FVector& TraceStart,
    const FVector& TraceEnd,
    const bool) const
{
    FVector MuzzleLocation = TraceStart;
    FRotator MuzzleRotation = (TraceEnd - TraceStart).Rotation();
    FTransform MuzzleTransform;
    if (ResolveMuzzleTransform(MuzzleTransform))
    {
        MuzzleLocation = MuzzleTransform.GetLocation();
        MuzzleRotation = MuzzleTransform.Rotator();
    }

    if (FireSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLocation);
    }
    if (MuzzleEffect)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this, MuzzleEffect, MuzzleLocation, MuzzleRotation);
    }
    if (FireMontage)
    {
        if (USkeletalMeshComponent* CharacterMesh = GetMesh())
        {
            if (UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance())
            {
                AnimInstance->Montage_Play(FireMontage);
            }
        }
    }
}

void UCQBTrainingAIComponent::ApplyRuntimeOptimization()
{
    AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(GetOwnerCharacter());
    if (!ShooterCharacter)
    {
        return;
    }

    if (USkeletalMeshComponent* FirstPersonMesh = ShooterCharacter->GetMesh1P_Implementation())
    {
        FirstPersonMesh->bPauseAnims = true;
        FirstPersonMesh->SetComponentTickEnabled(false);
    }
    if (UCameraComponent* ViewCamera = ShooterCharacter->GetViewCamera_Implementation())
    {
        ViewCamera->SetComponentTickEnabled(false);
    }
    if (USpringArmComponent* SpringArm = ShooterCharacter->FindComponentByClass<USpringArmComponent>())
    {
        SpringArm->SetComponentTickEnabled(false);
    }

    UCombatComponent* Combat = ShooterCharacter->GetCombatComponent();
    if (Combat)
    {
        Combat->SetComponentTickEnabled(false);
        SetWeaponRuntimeTickEnabled(Combat->GetWeaponInSlot(EWeaponSlot::Primary), false, false);
        SetWeaponRuntimeTickEnabled(Combat->GetWeaponInSlot(EWeaponSlot::Secondary), false, false);
        SetWeaponRuntimeTickEnabled(Combat->GetWeaponInSlot(EWeaponSlot::Melee), false, false);
    }
}

void UCQBTrainingAIComponent::PrepareForDeathCleanup(const float CorpseLifetimeSeconds)
{
    bDeathCleanupStarted = true;

    ACharacter* Character = GetOwnerCharacter();
    if (!Character)
    {
        return;
    }

    Character->SetActorTickEnabled(false);

    if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
    {
        Movement->StopMovementImmediately();
        Movement->DisableMovement();
        Movement->SetComponentTickEnabled(false);
    }

    USkeletalMeshComponent* BodyMesh = Character->GetMesh();
    if (BodyMesh)
    {
        BodyMesh->bPauseAnims = true;
    }

    TInlineComponentArray<UActorComponent*> CharacterComponents;
    Character->GetComponents(CharacterComponents);
    for (UActorComponent* Component : CharacterComponents)
    {
        if (IsValid(Component) && Component != BodyMesh)
        {
            Component->SetComponentTickEnabled(false);
        }
    }

    ApplyRuntimeOptimization();

    if (AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(Character))
    {
        if (UCombatComponent* Combat = ShooterCharacter->GetCombatComponent())
        {
            SetWeaponRuntimeTickEnabled(Combat->GetWeaponInSlot(EWeaponSlot::Primary), false, true);
            SetWeaponRuntimeTickEnabled(Combat->GetWeaponInSlot(EWeaponSlot::Secondary), false, true);
            SetWeaponRuntimeTickEnabled(Combat->GetWeaponInSlot(EWeaponSlot::Melee), false, true);
        }
    }

    Character->SetLifeSpan(FMath::Max(0.05f, CorpseLifetimeSeconds));
}
