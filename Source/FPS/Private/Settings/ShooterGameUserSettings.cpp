// Fill out your copyright notice in the Description page of Project Settings.

#include "Settings/ShooterGameUserSettings.h"

#include "Engine/Engine.h"

UShooterGameUserSettings* UShooterGameUserSettings::Get()
{
	return Cast<UShooterGameUserSettings>(GEngine->GetGameUserSettings());
}

void UShooterGameUserSettings::SetDynamicCrosshairEnabled(const bool bEnabled)
{
	bDynamicCrosshair = bEnabled;
	SaveSettings();
}