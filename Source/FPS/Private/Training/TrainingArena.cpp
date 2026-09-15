#include "Training/TrainingArena.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "Training/TrainingSessionSubsystem.h"
#include "Training/TrainingTarget.h"
#include "Training/AI/TrainingCombatAI.h"

ATrainingArena::ATrainingArena()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;

    CenterPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("CenterPoint"));
    SetRootComponent(CenterPoint);
    TrainingEntryPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("TrainingEntryPoint"));
    TrainingEntryPoint->SetupAttachment(CenterPoint);
    TrainingEntryPoint->SetRelativeLocation(FVector(0.0, 0.0, 100.0));

    MovementArea = CreateDefaultSubobject<USphereComponent>(TEXT("MovementArea"));
    MovementArea->SetupAttachment(CenterPoint);
    MovementArea->SetSphereRadius(TrainingRules::MovementRadiusCentimeters);
    MovementArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MovementArea->SetCanEverAffectNavigation(false);
    MovementArea->SetHiddenInGame(true);

    constexpr int32 SegmentCount = 32;
    for (int32 Index = 0; Index < SegmentCount; ++Index)
    {
        UBoxComponent* BoundaryPiece = CreateDefaultSubobject<UBoxComponent>(
            *FString::Printf(TEXT("MovementBoundary_%02d"), Index));
        BoundaryPiece->SetupAttachment(CenterPoint);
        BoundaryPiece->SetCollisionObjectType(ECC_WorldDynamic);
        BoundaryPiece->SetCollisionResponseToAllChannels(ECR_Ignore);
        BoundaryPiece->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        BoundaryPiece->SetGenerateOverlapEvents(false);
        BoundaryPiece->SetCanEverAffectNavigation(false);
        BoundaryPiece->SetHiddenInGame(true);
        MovementBoundary.Add(BoundaryPiece);
    }
    TargetClass = ATrainingTarget::StaticClass();
}

void ATrainingArena::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // Geometry uses world centimeters; scale the level art, not the arena coordinate frame.
    SetActorScale3D(FVector::OneVector);
    SetActorRotation(FRotator(0.0, GetActorRotation().Yaw, 0.0));
    MovementArea->SetSphereRadius(TrainingRules::MovementRadiusCentimeters);

    const float HalfStep = PI / static_cast<float>(MovementBoundary.Num());
    const float Apothem = TrainingRules::MovementRadiusCentimeters * FMath::Cos(HalfStep);
    const float HalfLength = TrainingRules::MovementRadiusCentimeters * FMath::Sin(HalfStep) + 2.0f;
    for (int32 Index = 0; Index < MovementBoundary.Num(); ++Index)
    {
        UBoxComponent* BoundaryPiece = MovementBoundary[Index];
        const float Angle = 2.0f * PI * static_cast<float>(Index) / MovementBoundary.Num();
        BoundaryPiece->SetRelativeLocation(FVector(
            (Apothem + 5.0f) * FMath::Cos(Angle),
            (Apothem + 5.0f) * FMath::Sin(Angle), 250.0));
        BoundaryPiece->SetRelativeRotation(FRotator(0.0, FMath::RadiansToDegrees(Angle), 0.0));
        BoundaryPiece->SetBoxExtent(FVector(5.0, HalfLength, 350.0));
        BoundaryPiece->SetCollisionEnabled(bEnableMovementBoundary
            ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }
}

void ATrainingArena::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATrainingArena, SessionState);
}

FVector ATrainingArena::GetCenterLocation() const
{
    return CenterPoint->GetComponentLocation();
}

FTransform ATrainingArena::GetEntryTransform() const
{
    return FTransform(TrainingEntryPoint->GetComponentRotation(),
        TrainingEntryPoint->GetComponentLocation(), FVector::OneVector);
}

bool ATrainingArena::IsInMovementArea(const AActor& Participant) const
{
    const FVector Center = GetCenterLocation();
    const FVector Position = Participant.GetActorLocation();
    return FVector::DistSquared2D(Center, Position)
            <= FMath::Square(TrainingRules::MovementRadiusCentimeters)
        && FMath::Abs(Position.Z - Center.Z) <= 600.0;
}

FVector ATrainingArena::MakeRadialCandidate(const FTrainingDistanceBand& Band) const
{
    const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
    const float Radius = FMath::FRandRange(Band.MinCentimeters, Band.MaxCentimeters);
    return GetCenterLocation() + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0);
}

bool ATrainingArena::FindGroundSpawn(const FTrainingDistanceBand& Band, FVector& OutGroundPosition) const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TrainingGround), false, this);
    for (int32 Attempt = 0; Attempt < 16; ++Attempt)
    {
        const FVector Candidate = MakeRadialCandidate(Band);
        FHitResult GroundHit;
        if (World->LineTraceSingleByObjectType(GroundHit,
            Candidate + FVector(0.0, 0.0, GroundProbeUpCentimeters),
            Candidate - FVector(0.0, 0.0, GroundProbeDownCentimeters),
            FCollisionObjectQueryParams(ECC_WorldStatic), QueryParams)
            && GroundHit.ImpactNormal.Z >= 0.7
            && Band.Contains(GetCenterLocation(), GroundHit.ImpactPoint))
        {
            OutGroundPosition = GroundHit.ImpactPoint;
            return true;
        }
    }
    return false;
}

bool ATrainingArena::ProjectNavPosition(const FVector& Candidate, const FTrainingDistanceBand& Band,
    const FNavAgentProperties& AgentProperties, FVector& OutNavPosition) const
{
    UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Navigation)
    {
        return false;
    }

    FNavLocation Projected;
    if (!Navigation->ProjectPointToNavigation(Candidate, Projected,
        NavigationProjectionExtent, &AgentProperties)
        || !Band.Contains(GetCenterLocation(), Projected.Location))
    {
        return false;
    }
    OutNavPosition = Projected.Location;
    return true;
}

bool ATrainingArena::FindNavSpawn(const FTrainingDistanceBand& Band,
    const FNavAgentProperties& AgentProperties, FVector& OutNavPosition) const
{
    for (int32 Attempt = 0; Attempt < 16; ++Attempt)
    {
        if (ProjectNavPosition(MakeRadialCandidate(Band), Band, AgentProperties, OutNavPosition))
        {
            return true;
        }
    }
    return false;
}

bool ATrainingArena::FindStrafeDestination(const FVector& CurrentPosition,
    const FTrainingDistanceBand& Band, const FNavAgentProperties& AgentProperties,
    const bool bClockwise, FVector& OutNavPosition) const
{
    const FVector Radial = (CurrentPosition - GetCenterLocation()).GetSafeNormal2D();
    if (Radial.IsNearlyZero())
    {
        return false;
    }
    const float Radius = FMath::Clamp(static_cast<float>(FVector::Dist2D(CurrentPosition, GetCenterLocation())),
        Band.MinCentimeters + 25.0f, Band.MaxCentimeters - 25.0f);
    const float StepDegrees = FMath::Clamp(FMath::RadiansToDegrees(150.0f / Radius), 1.0f, 12.0f);
    const float DirectionSign = bClockwise ? 1.0f : -1.0f;
    for (int32 Attempt = 0; Attempt < 4; ++Attempt)
    {
        const float Angle = DirectionSign * StepDegrees * (1.0f - 0.2f * Attempt);
        const FVector Candidate = GetCenterLocation() + Radial.RotateAngleAxis(Angle, FVector::UpVector) * Radius;
        if (ProjectNavPosition(Candidate, Band, AgentProperties, OutNavPosition)
            && Band.ContainsSegment(GetCenterLocation(), CurrentPosition, OutNavPosition))
        {
            return true;
        }
    }
    return false;
}

void ATrainingArena::SetSessionState(const ETrainingSessionState NewState)
{
    if (HasAuthority())
    {
        SessionState = NewState;
        ForceNetUpdate();
    }
}

void ATrainingArena::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UTrainingSessionSubsystem* Sessions = GetWorld()->GetSubsystem<UTrainingSessionSubsystem>())
    {
        Sessions->EndSessionForArena(this);
    }
    Super::EndPlay(EndPlayReason);
}
