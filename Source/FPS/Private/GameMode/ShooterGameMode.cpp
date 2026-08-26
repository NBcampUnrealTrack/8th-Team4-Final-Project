// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/ShooterGameMode.h"

AShooterGameMode::AShooterGameMode()
{
	bUseSeamlessTravel = true;
}

void AShooterGameMode::NotifyPlayerEliminatedBy(
	AController*,
	AShooterPlayerState*)
{
}

void AShooterGameMode::NotifyPlayerReadyForGameFlow(APlayerController*)
{
}
