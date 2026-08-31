// ShooterUISettings.cpp
#include "UI/ShooterUISettings.h"

FLinearColor UShooterUISettings::GetAllyColor()
{
	return GetDefault<UShooterUISettings>()->AllyColor;
}

FLinearColor UShooterUISettings::GetEnemyColor()
{
	return GetDefault<UShooterUISettings>()->EnemyColor;
}
