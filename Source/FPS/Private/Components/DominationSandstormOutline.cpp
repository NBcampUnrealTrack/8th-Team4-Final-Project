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
	const UDominationSandstormSettings* OutlineSettings = IsValid(Settings.Get())
		? Settings.Get() : GetDefault<UDominationSandstormSettings>();
	const double RangeCm = static_cast<double>(OutlineSettings->EnemyOutlineVisibleDistanceMeters) * 100.0;
	if (!FMath::IsFinite(RangeCm) || RangeCm <= 0.0)
	{
		return false;
	}

	FVector ViewerLocation;
	const APawn* ViewerPawn = Controller->GetPawn();
	if (IsValid(ViewerPawn) && !ViewerPawn->IsA<ASpectatorPawn>()
		&& Controller->GetViewTarget() == ViewerPawn)
	{
		// ADS 배율과 관계없이 실제 플레이어 위치를 기준으로 한다.
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

bool UDominationSandstormComponent::ShouldLimitEnemyOutline() const
{
	const APlayerController* Controller = LocalController.Get();
	const ADominationGameState* GameState = BoundGameState.Get();
	if (!IsValid(Controller) || !Controller->IsLocalController() || !IsValid(GameState)
		|| GameState->GetWorld() != GetWorld()
		|| GameState->GetDominationPhase() == EDominationMatchPhase::MatchEnded)
	{
		return false;
	}

#if !UE_BUILD_SHIPPING
	// 미리보기에서도 화면의 폭풍과 동일하게 제한/복구한다.
	const float Preview = LocalPreviewIntensity >= 0.0f ? LocalPreviewIntensity
		: (IsValid(Settings.Get()) ? Settings->PreviewIntensity : -1.0f);
	if (Preview >= 0.0f)
	{
		return Preview > UE_SMALL_NUMBER;
	}
#endif

	using EPhase = EDominationSandstormPhase;
	return CurrentState.Phase == EPhase::FadeIn || CurrentState.Phase == EPhase::Peak
		|| CurrentState.Phase == EPhase::FadeOut;
}

void UDominationSandstormComponent::UpdateOutlineDistanceLimit()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !ShouldLimitEnemyOutline())
	{
		StopOutlineDistanceLimit();
		return;
	}

	if (bOutlineDistanceLimited && OutlineTrackingWorld.Get() != World)
	{
		StopOutlineDistanceLimit();
	}

	if (!bOutlineDistanceLimited)
	{
		bOutlineDistanceLimited = true;
		OutlineTrackingWorld = World;
		OutlineActorSpawnedHandle = World->AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::TrackOutlineActor));
		OutlineLevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(
			this, &ThisClass::HandleOutlineLevelAdded);

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

	const UDominationSandstormSettings* OutlineSettings = IsValid(Settings.Get())
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
	if (OutlineLevelAddedHandle.IsValid())
	{
		FWorldDelegates::LevelAddedToWorld.Remove(OutlineLevelAddedHandle);
	}
	OutlineUpdateTimer.Invalidate();
	OutlineActorSpawnedHandle.Reset();
	OutlineLevelAddedHandle.Reset();
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
	if (bOutlineDistanceLimited && !ShouldLimitEnemyOutline())
	{
		StopOutlineDistanceLimit();
		return;
	}
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
		// 거리 타이머는 변경 시에만 적용하고, 종료할 때는 부착물까지 강제로 복구한다.
		if (AShooterCharacter* Character = Cast<AShooterCharacter>(Actor))
		{
			Character->RefreshTeamOutline(!bOutlineDistanceLimited);
		}
		else if (ARogueDeployableActor* Deployable = Cast<ARogueDeployableActor>(Actor))
		{
			Deployable->RefreshTeamOutline(!bOutlineDistanceLimited);
		}
	}
}
