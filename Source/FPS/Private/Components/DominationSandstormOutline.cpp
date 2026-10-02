#include "Components/DominationSandstormComponent.h"

#include "Augment/Actor/RogueDeployableActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/ShooterCharacter.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameMode/Settings/DominationSandstormSettings.h"
#include "GameMode/State/DominationGameState.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "TimerManager.h"

bool UDominationSandstormComponent::IsEnemyOutlineInRange(const AActor& Target) const
{
	if (!bOutlineDistanceLimited)
	{
		return true;
	}

	const APlayerController* Controller = LocalController.Get();
	if (!IsValid(Controller) || !Controller->IsLocalController() || Target.GetWorld() != GetWorld())
	{
		return false;
	}

	// 상태가 설정 에셋보다 먼저 복제되면 기본 거리로 제한한다.
	const UDominationSandstormSettings* OutlineSettings = Settings
		? Settings.Get() : GetDefault<UDominationSandstormSettings>();
	const double RangeCm = static_cast<double>(OutlineSettings->EnemyOutlineVisibleDistanceMeters) * 100.0;
	if (!FMath::IsFinite(RangeCm) || RangeCm <= 0.0)
	{
		return false;
	}

	FVector ViewerLocation;
	const APawn* ViewerPawn = Controller->GetPawn();
	if (IsValid(ViewerPawn) && !ViewerPawn->IsA<ASpectatorPawn>())
	{
		ViewerLocation = ViewerPawn->GetActorLocation();
	}
	else if (IsValid(Controller->PlayerCameraManager))
	{
		// 사망/관전 중에는 남아 있는 사망 지점이 아니라 실제 관전 시점을 사용한다.
		ViewerLocation = Controller->PlayerCameraManager->GetCameraLocation();
	}
	else
	{
		return false;
	}

	return FVector::DistSquared(ViewerLocation, Target.GetActorLocation()) <= FMath::Square(RangeCm);
}

void UDominationSandstormComponent::UpdateOutlineDistanceLimit()
{
	UWorld* World = GetWorld();
	const ADominationGameState* GameState = BoundGameState.Get();
	using EPhase = EDominationSandstormPhase;
	const bool bStormPhase = CurrentState.Phase == EPhase::FadeIn
		|| CurrentState.Phase == EPhase::Peak || CurrentState.Phase == EPhase::FadeOut;
	const bool bShouldLimit = IsValid(World) && LocalController.IsValid()
		&& LocalController->IsLocalController() && IsValid(GameState)
		&& GameState->GetDominationPhase() != EDominationMatchPhase::MatchEnded && bStormPhase;
	if (!bShouldLimit)
	{
		StopOutlineDistanceLimit();
		return;
	}

	if (!bOutlineDistanceLimited)
	{
		bOutlineDistanceLimited = true;
		OutlineTrackingWorld = World;
		OutlineActorSpawnedHandle = World->AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::TrackOutlineActor));

		// 시작 시에만 탐색하고 이후 스폰/스트리밍 이벤트로 목록을 보완한다.
		for (TActorIterator<AShooterCharacter> It(World); It; ++It)
		{
			TrackOutlineActor(*It);
		}
		for (TActorIterator<ARogueDeployableActor> It(World); It; ++It)
		{
			TrackOutlineActor(*It);
		}
	}

	const UDominationSandstormSettings* OutlineSettings = Settings
		? Settings.Get() : GetDefault<UDominationSandstormSettings>();
	const float ConfiguredInterval = OutlineSettings->EnemyOutlineUpdateInterval;
	const float Interval = FMath::IsFinite(ConfiguredInterval)
		? FMath::Clamp(ConfiguredInterval, 0.02f, 0.25f)
		: GetDefault<UDominationSandstormSettings>()->EnemyOutlineUpdateInterval;
	FTimerManager& Timers = World->GetTimerManager();
	if (!Timers.IsTimerActive(OutlineUpdateTimer)
		|| !FMath::IsNearlyEqual(Timers.GetTimerRate(OutlineUpdateTimer), Interval))
	{
		FTimerManagerTimerParameters Parameters;
		Parameters.bLoop = true;
		Parameters.bMaxOncePerFrame = true;
		Parameters.FirstDelay = Interval;
		Timers.SetTimer(OutlineUpdateTimer, this, &ThisClass::RefreshOutlineTargets, Interval, Parameters);
	}
	// 단계/설정 변경은 다음 거리 타이머까지 기다리지 않는다.
	RefreshOutlineTargets();
}

void UDominationSandstormComponent::StopOutlineDistanceLimit()
{
	// 복구 시 기존 아웃라인 함수가 더 이상 거리 제한을 적용하지 않게 먼저 해제한다.
	bOutlineDistanceLimited = false;
	if (UWorld* World = OutlineTrackingWorld.Get())
	{
		World->GetTimerManager().ClearTimer(OutlineUpdateTimer);
		if (OutlineActorSpawnedHandle.IsValid())
		{
			World->RemoveOnActorSpawnedHandler(OutlineActorSpawnedHandle);
		}
	}
	OutlineUpdateTimer.Invalidate();
	OutlineActorSpawnedHandle.Reset();
	RefreshOutlineTargets();
	TrackedOutlineActors.Reset();
	OutlineTrackingWorld.Reset();
}

void UDominationSandstormComponent::TrackOutlineActor(AActor* Actor)
{
	if (bOutlineDistanceLimited && IsValid(Actor) && Actor->GetWorld() == OutlineTrackingWorld.Get()
		&& (Actor->IsA<AShooterCharacter>() || Actor->IsA<ARogueDeployableActor>()))
	{
		// Deferred spawn도 있으므로 이 콜백에서는 생성 중인 컴포넌트에 접근하지 않는다.
		TrackedOutlineActors.AddUnique(TWeakObjectPtr<AActor>(Actor));
	}
}

void UDominationSandstormComponent::HandleOutlineLevelAdded(ULevel* Level, UWorld* World)
{
	if (!bOutlineDistanceLimited || !Level || World != OutlineTrackingWorld.Get())
	{
		return;
	}
	for (AActor* Actor : Level->Actors)
	{
		TrackOutlineActor(Actor);
	}
	RefreshOutlineTargets();
}

void UDominationSandstormComponent::RefreshOutlineTargets()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DominationSandstorm_Outline);
	for (int32 Index = TrackedOutlineActors.Num() - 1; Index >= 0; --Index)
	{
		AActor* Actor = TrackedOutlineActors[Index].Get();
		if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetWorld() != OutlineTrackingWorld.Get())
		{
			TrackedOutlineActors.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			continue;
		}
		if (!Actor->HasActorBegunPlay())
		{
			continue;
		}
		// 거리/팀/발각 결과가 같으면 메시 순회와 렌더 상태 변경을 모두 생략한다.
		if (AShooterCharacter* Character = Cast<AShooterCharacter>(Actor))
		{
			Character->RefreshTeamOutline(false);
		}
		else if (ARogueDeployableActor* Deployable = Cast<ARogueDeployableActor>(Actor))
		{
			Deployable->RefreshTeamOutline(false);
		}
	}
}
