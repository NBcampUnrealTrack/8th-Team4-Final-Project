// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/ShooterGameMode.h"

#include "EngineUtils.h"
#include "GAS/Damage/RogueDamageTypes.h"
#include "GameFramework/GameStateBase.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpectatorPawn.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterSpectatorPawn.h"
#include "Player/ShooterPlayerState.h"

namespace
{
	AShooterPlayerState* ResolveShooterPlayerState(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return nullptr;
		}

		if (AShooterPlayerState* PlayerState = Cast<AShooterPlayerState>(Actor))
		{
			return PlayerState;
		}
		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			return Pawn->GetPlayerState<AShooterPlayerState>();
		}
		if (AController* Controller = Cast<AController>(Actor))
		{
			return Controller->GetPlayerState<AShooterPlayerState>();
		}

		return nullptr;
	}
}

AShooterGameMode::AShooterGameMode()
{
	bUseSeamlessTravel = true;
	ShooterSpectatorPawnClass = AShooterSpectatorPawn::StaticClass();
}

void AShooterGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	InitializePlayerParticipationState(NewPlayer);
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void AShooterGameMode::HandlePlayerEliminated(
	AController* EliminatedController,
	const FRogueDeathContext& DeathContext)
{
	if (!HasAuthority() || !IsValid(EliminatedController))
	{
		return;
	}

	AShooterPlayerState* KillerPlayerState = ResolveKillerPlayerState(DeathContext);
	AShooterPlayerState* VictimPlayerState = EliminatedController->GetPlayerState<AShooterPlayerState>();

	if (IsValid(VictimPlayerState))
	{
		VictimPlayerState->ServerAddDeath();
	}

	// 자살/팀킬은 킬 증가 X
	if (IsValid(KillerPlayerState)
		&& KillerPlayerState != VictimPlayerState
		&& (!IsValid(VictimPlayerState) || KillerPlayerState->GetTeam() != VictimPlayerState->GetTeam()))
	{
		KillerPlayerState->ServerAddKill();
	}

	if (ABaseTeamGameState* TeamGameState = GetGameState<ABaseTeamGameState>())
	{
		FShooterEliminationEvent EliminationEvent;
		EliminationEvent.KillerPlayerState = KillerPlayerState;
		EliminationEvent.VictimPlayerState = EliminatedController->GetPlayerState<AShooterPlayerState>();
		EliminationEvent.WeaponTag = DeathContext.WeaponTag;
		EliminationEvent.bHeadshot = DeathContext.bHeadshot;

		TeamGameState->BroadcastPlayerEliminated(EliminationEvent);
	}

	NotifyPlayerEliminatedBy(EliminatedController, KillerPlayerState);
}

AShooterPlayerState* AShooterGameMode::ResolveKillerPlayerState(const FRogueDeathContext& DeathContext)
{
	if (AShooterPlayerState* PlayerState = ResolveShooterPlayerState(DeathContext.InstigatorActor.Get()))
	{
		return PlayerState;
	}

	AActor* Causer = DeathContext.CauserActor.Get();
	if (AShooterPlayerState* PlayerState = ResolveShooterPlayerState(Causer))
	{
		return PlayerState;
	}
	if (!IsValid(Causer))
	{
		return nullptr;
	}

	if (AShooterPlayerState* PlayerState = ResolveShooterPlayerState(Causer->GetOwner()))
	{
		return PlayerState;
	}
	return ResolveShooterPlayerState(Causer->GetInstigator());
}

void AShooterGameMode::NotifyPlayerEliminatedBy(
	AController*,
	AShooterPlayerState*)
{
}

void AShooterGameMode::NotifyPlayerReadyForGameFlow(APlayerController*)
{
}

EPlayerParticipationState AShooterGameMode::ResolveParticipationStateForNewPlayer(APlayerController*) const
{
	return EPlayerParticipationState::Active;
}

void AShooterGameMode::InitializePlayerParticipationState(APlayerController* NewPlayer) const
{
	if (AShooterPlayerState* PlayerState = IsValid(NewPlayer)
		? NewPlayer->GetPlayerState<AShooterPlayerState>()
		: nullptr)
	{
		PlayerState->SetParticipationState(ResolveParticipationStateForNewPlayer(NewPlayer));
	}
}

void AShooterGameMode::MovePlayerToSpectator(APlayerController* PlayerController)
{
	if (!HasAuthority() || !IsValid(PlayerController))
	{
		return;
	}

	const FTransform SpawnTransform = ResolveSpectatorSpawnTransform(PlayerController);

	if (APawn* ExistingPawn = PlayerController->GetPawn())
	{
		PlayerController->UnPossess();
		ExistingPawn->Destroy();
	}

	ASpectatorPawn* SpectatorPawn = SpawnSpectatorPawnFor(PlayerController, SpawnTransform);
	if (!IsValid(SpectatorPawn))
	{
		return;
	}

	PlayerController->ChangeState(NAME_Spectating);
	PlayerController->ClientGotoState(NAME_Spectating);
	PlayerController->Possess(SpectatorPawn);

	if (AShooterPlayerController* ShooterPlayerController = Cast<AShooterPlayerController>(PlayerController))
	{
		ShooterPlayerController->ClientEnterSpectatorMode(nullptr);
	}
	CycleSpectatorTarget(PlayerController);
}

void AShooterGameMode::CycleSpectatorTarget(APlayerController* SpectatorController)
{
	if (!HasAuthority() || !IsValid(SpectatorController))
	{
		return;
	}

	APawn* TargetPawn = FindNextSpectatorTarget(SpectatorController);
	if (!IsValid(TargetPawn))
	{
		return;
	}

	FViewTargetTransitionParams TransitionParams;
	TransitionParams.BlendTime = 0.15f;
	SpectatorController->ClientSetViewTarget(TargetPawn, TransitionParams);

	if (AShooterPlayerController* ShooterPlayerController = Cast<AShooterPlayerController>(SpectatorController))
	{
		ShooterPlayerController->ClientSetSpectatorTarget(TargetPawn->GetPlayerState());
	}
}

ASpectatorPawn* AShooterGameMode::SpawnSpectatorPawnFor(
	APlayerController* PlayerController,
	const FTransform& SpawnTransform)
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(PlayerController))
	{
		return nullptr;
	}

	TSubclassOf<ASpectatorPawn> ClassToSpawn = ShooterSpectatorPawnClass;
	if (!ClassToSpawn)
	{
		ClassToSpawn = AShooterSpectatorPawn::StaticClass();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = PlayerController;
	SpawnParameters.Instigator = PlayerController->GetPawn();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	return World->SpawnActor<ASpectatorPawn>(
		ClassToSpawn,
		SpawnTransform.GetLocation(),
		SpawnTransform.Rotator(),
		SpawnParameters);
}

APawn* AShooterGameMode::FindNextSpectatorTarget(APlayerController* SpectatorController) const
{
	const AGameStateBase* CurrentGameState = GameState;
	const AShooterPlayerState* SpectatorPlayerState = IsValid(SpectatorController)
		? SpectatorController->GetPlayerState<AShooterPlayerState>()
		: nullptr;
	if (!IsValid(CurrentGameState) || !IsValid(SpectatorPlayerState) || SpectatorPlayerState->GetTeam() == EGameTeam::None)
	{
		return nullptr;
	}

	TArray<APawn*> CandidatePawns;
	for (APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		const AShooterPlayerState* CandidatePlayerState = Cast<AShooterPlayerState>(PlayerState);
		if (!IsValid(CandidatePlayerState)
			|| CandidatePlayerState == SpectatorPlayerState
			|| CandidatePlayerState->GetTeam() != SpectatorPlayerState->GetTeam()
			|| !CandidatePlayerState->IsAlive()
			|| CandidatePlayerState->GetParticipationState() != EPlayerParticipationState::Active)
		{
			continue;
		}

		APawn* CandidatePawn = CandidatePlayerState->GetPawn();
		if (IsValid(CandidatePawn))
		{
			CandidatePawns.Add(CandidatePawn);
		}
	}

	if (CandidatePawns.IsEmpty())
	{
		return nullptr;
	}

	const AActor* CurrentViewTarget = SpectatorController->GetViewTarget();
	const int32 CurrentIndex = CandidatePawns.IndexOfByKey(CurrentViewTarget);
	if (CurrentIndex == INDEX_NONE)
	{
		return CandidatePawns[0];
	}

	return CandidatePawns[(CurrentIndex + 1) % CandidatePawns.Num()];
}

FTransform AShooterGameMode::ResolveSpectatorSpawnTransform(APlayerController* PlayerController) const
{
	if (IsValid(PlayerController))
	{
		if (const APawn* CurrentPawn = PlayerController->GetPawn())
		{
			return CurrentPawn->GetActorTransform();
		}
	}

	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<APlayerStart> Iterator(World); Iterator; ++Iterator)
		{
			const APlayerStart* PlayerStart = *Iterator;
			if (IsValid(PlayerStart))
			{
				return PlayerStart->GetActorTransform();
			}
		}
	}

	return FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 300.0f));
}
