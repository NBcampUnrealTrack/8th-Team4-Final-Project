#include "Minimap/ShooterMinimapSubsystem.h"

#include "CollisionQueryParams.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Minimap/MinimapTypes.h"
#include "Minimap/ShooterMinimapBounds.h"
#include "Player/BaseTeamPlayerState.h"

UShooterMinimapSubsystem* UShooterMinimapSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UShooterMinimapSubsystem>() : nullptr;
}

bool UShooterMinimapSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UShooterMinimapSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	for (TActorIterator<AShooterMinimapBounds> It(&InWorld); It; ++It)
	{
		RegisterMapBounds(*It);
		break;
	}

	// 시야 판정은 서버만 한다. 클라는 복제된 결과만 본다.
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}

	InWorld.GetTimerManager().SetTimer(
		SpotCheckTimerHandle,
		this,
		&ThisClass::EvaluateSpotting,
		ShooterMinimapLimits::SpotCheckIntervalSeconds,
		true);
}

void UShooterMinimapSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpotCheckTimerHandle);
	}

	MapBounds.Reset();
	OnMapBoundsChanged.Clear();

	Super::Deinitialize();
}

bool UShooterMinimapSubsystem::IsKillcamDuplicate(const AShooterMinimapBounds* Bounds)
{
	const ULevel* Level = Bounds ? Bounds->GetLevel() : nullptr;
	const FLevelCollection* Collection = Level ? Level->GetCachedLevelCollection() : nullptr;
	return Collection && Collection->GetType() == ELevelCollectionType::DynamicDuplicatedLevels;
}

AShooterMinimapBounds* UShooterMinimapSubsystem::ResolveMapBounds()
{
	if (MapBounds.IsValid())
	{
		return MapBounds.Get();
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<AShooterMinimapBounds> It(World); It; ++It)
	{
		if (IsValid(*It) && It->HasActorBegunPlay() && !IsKillcamDuplicate(*It))
		{
			RegisterMapBounds(*It);
			break;
		}
	}

	return MapBounds.Get();
}

void UShooterMinimapSubsystem::RegisterMapBounds(AShooterMinimapBounds* Bounds)
{
	// 킬캠 사본이 기준을 차지하면 재생 종료 때 Unregister되면서 실제 맵의 기준까지 사라진다.
	if (!IsValid(Bounds) || MapBounds.Get() == Bounds || IsKillcamDuplicate(Bounds))
	{
		return;
	}

	if (MapBounds.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ShooterMinimapSubsystem] MinimapBounds가 레벨에 둘 이상 있습니다. 마지막 것을 사용합니다: %s"),
			*Bounds->GetName());
	}

	MapBounds = Bounds;
	OnMapBoundsChanged.Broadcast(Bounds);
}

void UShooterMinimapSubsystem::UnregisterMapBounds(AShooterMinimapBounds* Bounds)
{
	if (MapBounds.Get() != Bounds)
	{
		return;
	}

	MapBounds.Reset();
	OnMapBoundsChanged.Broadcast(nullptr);
}

void UShooterMinimapSubsystem::EvaluateSpotting()
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (GameState == nullptr)
	{
		return;
	}

	struct FSpotParticipant
	{
		ABaseTeamPlayerState* PlayerState;
		const APawn* Pawn;
		EGameTeam Team;
	};

	// 1. 판정 대상은 팀이 있고 살아있고 Pawn이 있는 플레이어만. 나머지는 즉시 해제.
	TArray<FSpotParticipant, TInlineAllocator<16>> Participants;
	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		ABaseTeamPlayerState* TeamPlayerState = Cast<ABaseTeamPlayerState>(PlayerState);
		if (!IsValid(TeamPlayerState))
		{
			continue;
		}

		const APawn* Pawn = TeamPlayerState->GetPawn();
		const EGameTeam Team = TeamPlayerState->GetTeam();
		if (Team == EGameTeam::None || !TeamPlayerState->IsAlive() || !IsValid(Pawn))
		{
			TeamPlayerState->SetSpottedByEnemy(false);
			continue;
		}

		Participants.Add({ TeamPlayerState, Pawn, Team });
	}

	// 2. 대상마다 상대 팀 관측자 중 한 명이라도 보면 발견. 값이 바뀔 때만 복제되므로 매 주기 Set해도 된다.
	for (const FSpotParticipant& Target : Participants)
	{
		bool bSpotted = false;
		for (const FSpotParticipant& Observer : Participants)
		{
			if (Observer.Team == Target.Team)
			{
				continue;
			}

			if (CanObserverSee(Observer.Pawn, Target.Pawn))
			{
				bSpotted = true;
				break;
			}
		}

		Target.PlayerState->SetSpottedByEnemy(bSpotted);
	}
}

bool UShooterMinimapSubsystem::CanObserverSee(const APawn* Observer, const APawn* Target) const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	Observer->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	const FVector ViewDirection = EyeRotation.Vector();

	const float MaxDistanceSq = FMath::Square(ShooterMinimapLimits::SpotMaxDistance);
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(ShooterMinimapLimits::SpotHalfFovDegrees));

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MinimapSpotting), false, Observer);
	QueryParams.AddIgnoredActor(Target);

	// 머리만 빼꼼 내민 경우와 몸통만 보이는 경우를 둘 다 잡는다.
	const FVector TargetPoints[] = { Target->GetPawnViewLocation(), Target->GetActorLocation() };
	for (const FVector& TargetPoint : TargetPoints)
	{
		const FVector ToTarget = TargetPoint - EyeLocation;
		if (ToTarget.SizeSquared() > MaxDistanceSq)
		{
			continue;
		}

		if (FVector::DotProduct(ViewDirection, ToTarget.GetSafeNormal()) < MinDot)
		{
			continue;
		}

		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, EyeLocation, TargetPoint, ECC_Visibility, QueryParams))
		{
			return true;
		}
	}

	return false;
}
