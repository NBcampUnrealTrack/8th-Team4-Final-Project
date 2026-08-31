// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/ShooterGameMode.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpectatorPawn.h"
#include "Player/ShooterSpectatorPawn.h"
#include "Player/ShooterPlayerState.h"

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
