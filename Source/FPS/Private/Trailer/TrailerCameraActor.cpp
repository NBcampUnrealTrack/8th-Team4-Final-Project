#include "Trailer/TrailerCameraActor.h"

#include "CineCameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetMathLibrary.h"

ATrailerCameraActor::ATrailerCameraActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetCanBeDamaged(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	CineCamera = CreateDefaultSubobject<UCineCameraComponent>(TEXT("CineCamera"));
	CineCamera->SetupAttachment(Root);
	// 촬영용 기본 렌즈. 콘솔로 바꿀 수 있다.
	CineCamera->CurrentFocalLength = 35.f;
	CineCamera->CurrentAperture = 2.8f;
}

void ATrailerCameraActor::SetMode(const ETrailerCameraMode NewMode)
{
	Mode = NewMode;
}

void ATrailerCameraActor::GatherTargets(TArray<APawn*>& OutTargets) const
{
	OutTargets.Reset();

	const UWorld* World = GetWorld();
	const APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		APawn* Pawn = IsValid(PlayerState) ? PlayerState->GetPawn() : nullptr;
		// 관전 폰이나 본인은 제외하고 실제 캐릭터만 대상으로 삼는다.
		if (!IsValid(Pawn) || !Pawn->IsA<ACharacter>() || (LocalController && Pawn->GetController() == LocalController))
		{
			continue;
		}
		OutTargets.Add(Pawn);
	}

	// 접속 순서와 무관하게 번호가 안정적이도록 PlayerId로 정렬한다.
	OutTargets.Sort([](const APawn& A, const APawn& B)
	{
		const int32 IdA = A.GetPlayerState() ? A.GetPlayerState()->GetPlayerId() : 0;
		const int32 IdB = B.GetPlayerState() ? B.GetPlayerState()->GetPlayerId() : 0;
		return IdA < IdB;
	});
}

APawn* ATrailerCameraActor::ResolveTarget() const
{
	TArray<APawn*> Targets;
	GatherTargets(Targets);
	if (Targets.IsEmpty())
	{
		return nullptr;
	}

	const int32 Count = Targets.Num();
	const int32 Index = ((TargetIndex % Count) + Count) % Count;
	return Targets[Index];
}

void ATrailerCameraActor::CycleTarget(const int32 Direction)
{
	TargetIndex += Direction >= 0 ? 1 : -1;
}

void ATrailerCameraActor::SetTargetIndex(const int32 NewIndex)
{
	TargetIndex = NewIndex;
}

bool ATrailerCameraActor::SetParameter(const FString& Name, const float Value)
{
	const FString Key = Name.ToLower();

	if (Key == TEXT("distance")) { Distance = FMath::Max(0.f, Value); }
	else if (Key == TEXT("height")) { Height = Value; }
	else if (Key == TEXT("side")) { SideOffset = Value; }
	else if (Key == TEXT("lookheight")) { LookHeight = Value; }
	else if (Key == TEXT("orbitspeed")) { OrbitSpeed = Value; }
	else if (Key == TEXT("speed")) { MoveSpeed = FMath::Max(10.f, Value); }
	else if (Key == TEXT("look")) { LookSensitivity = FMath::Max(0.01f, Value); }
	else if (Key == TEXT("smooth")) { PositionSmoothing = FMath::Max(0.f, Value); RotationSmoothing = FMath::Max(0.f, Value * 1.3f); }
	else if (Key == TEXT("focal")) { CineCamera->SetCurrentFocalLength(FMath::Clamp(Value, 4.f, 1000.f)); }
	else if (Key == TEXT("aperture")) { CineCamera->SetCurrentAperture(FMath::Clamp(Value, 1.2f, 32.f)); }
	else if (Key == TEXT("dof")) { bDepthOfField = Value > 0.5f; }
	else { return false; }

	return true;
}

FString ATrailerCameraActor::DescribeState() const
{
	const TCHAR* ModeName = Mode == ETrailerCameraMode::Free ? TEXT("Free")
		: Mode == ETrailerCameraMode::Follow ? TEXT("Follow") : TEXT("Orbit");

	TArray<APawn*> Targets;
	GatherTargets(Targets);
	const APawn* Target = ResolveTarget();

	return FString::Printf(
		TEXT("[TrailerCam] Mode=%s Target=%s(%d/%d) Focal=%.0fmm F%.1f Dist=%.0f Height=%.0f Side=%.0f OrbitSpeed=%.0f Speed=%.0f DOF=%d"),
		ModeName, Target ? *Target->GetName() : TEXT("없음"), TargetIndex, Targets.Num(),
		CineCamera->CurrentFocalLength, CineCamera->CurrentAperture,
		Distance, Height, SideOffset, OrbitSpeed, MoveSpeed, bDepthOfField);
}

void ATrailerCameraActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;

	switch (Mode)
	{
	case ETrailerCameraMode::Free:
		TickFree(PlayerController, DeltaSeconds);
		break;
	case ETrailerCameraMode::Follow:
		TickFollow(DeltaSeconds);
		break;
	case ETrailerCameraMode::Orbit:
		TickOrbit(DeltaSeconds);
		break;
	}
}

void ATrailerCameraActor::TickFree(APlayerController* PlayerController, const float DeltaSeconds)
{
	if (!PlayerController)
	{
		return;
	}

	// 마우스: 시점 회전. 롤은 항상 0으로 유지한다.
	FRotator Rotation = GetActorRotation();
	Rotation.Yaw += PlayerController->GetInputAnalogKeyState(EKeys::MouseX) * LookSensitivity;
	Rotation.Pitch = FMath::Clamp(
		Rotation.Pitch + PlayerController->GetInputAnalogKeyState(EKeys::MouseY) * LookSensitivity, -89.f, 89.f);
	Rotation.Roll = 0.f;

	// 휠: 이동 속도 조절
	if (PlayerController->WasInputKeyJustPressed(EKeys::MouseScrollUp)) { MoveSpeed = FMath::Min(MoveSpeed * 1.25f, 8000.f); }
	if (PlayerController->WasInputKeyJustPressed(EKeys::MouseScrollDown)) { MoveSpeed = FMath::Max(MoveSpeed * 0.8f, 20.f); }

	float Speed = MoveSpeed;
	if (PlayerController->IsInputKeyDown(EKeys::LeftShift)) { Speed *= 4.f; }
	if (PlayerController->IsInputKeyDown(EKeys::LeftControl)) { Speed *= 0.25f; }

	const FVector Forward = Rotation.Vector();
	const FVector Right = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y);

	FVector Move = FVector::ZeroVector;
	if (PlayerController->IsInputKeyDown(EKeys::W)) { Move += Forward; }
	if (PlayerController->IsInputKeyDown(EKeys::S)) { Move -= Forward; }
	if (PlayerController->IsInputKeyDown(EKeys::D)) { Move += Right; }
	if (PlayerController->IsInputKeyDown(EKeys::A)) { Move -= Right; }
	if (PlayerController->IsInputKeyDown(EKeys::E)) { Move += FVector::UpVector; }
	if (PlayerController->IsInputKeyDown(EKeys::Q)) { Move -= FVector::UpVector; }

	SetActorLocationAndRotation(GetActorLocation() + Move.GetSafeNormal() * Speed * DeltaSeconds, Rotation);
}

void ATrailerCameraActor::ApplyLookAt(const FVector& LookAtLocation, const float DeltaSeconds, const bool bSmooth)
{
	FRotator Desired = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), LookAtLocation);
	Desired.Roll = 0.f;

	const FRotator NewRotation = (bSmooth && RotationSmoothing > 0.f)
		? FMath::RInterpTo(GetActorRotation(), Desired, DeltaSeconds, RotationSmoothing)
		: Desired;
	SetActorRotation(NewRotation);
}

void ATrailerCameraActor::UpdateFocus(const FVector& LookAtLocation)
{
	if (bDepthOfField)
	{
		CineCamera->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
		CineCamera->FocusSettings.ManualFocusDistance = FVector::Distance(GetActorLocation(), LookAtLocation);
	}
	else
	{
		CineCamera->FocusSettings.FocusMethod = ECameraFocusMethod::DoNotOverride;
	}
}

void ATrailerCameraActor::TickFollow(const float DeltaSeconds)
{
	const APawn* Target = ResolveTarget();
	if (!Target)
	{
		return;
	}

	const FVector LookAt = Target->GetActorLocation() + FVector(0.f, 0.f, LookHeight);

	// 캐릭터가 바라보는 수평 방향의 뒤쪽/옆쪽 위치를 목표로 부드럽게 따라간다.
	const FRotator YawOnly(0.f, Target->GetActorRotation().Yaw, 0.f);
	const FVector Back = -YawOnly.Vector();
	const FVector Right = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);
	const FVector DesiredLocation = LookAt + Back * Distance + Right * SideOffset + FVector(0.f, 0.f, Height - LookHeight);

	const FVector NewLocation = PositionSmoothing > 0.f
		? FMath::VInterpTo(GetActorLocation(), DesiredLocation, DeltaSeconds, PositionSmoothing)
		: DesiredLocation;
	SetActorLocation(NewLocation);

	ApplyLookAt(LookAt, DeltaSeconds, true);
	UpdateFocus(LookAt);
}

void ATrailerCameraActor::TickOrbit(const float DeltaSeconds)
{
	const APawn* Target = ResolveTarget();
	if (!Target)
	{
		return;
	}

	OrbitAngleDegrees = FMath::Fmod(OrbitAngleDegrees + OrbitSpeed * DeltaSeconds, 360.f);

	const FVector LookAt = Target->GetActorLocation() + FVector(0.f, 0.f, LookHeight);
	const float Radians = FMath::DegreesToRadians(OrbitAngleDegrees);
	const FVector Offset(FMath::Cos(Radians) * Distance, FMath::Sin(Radians) * Distance, Height - LookHeight);

	SetActorLocation(LookAt + Offset);
	ApplyLookAt(LookAt, DeltaSeconds, false);
	UpdateFocus(LookAt);
}
