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

FLinearColor UShooterUISettings::GetLobbyTeamColor(const EGameTeam Team)
{
	const UShooterUISettings* Settings = GetDefault<UShooterUISettings>();
	switch (Team)
	{
	case EGameTeam::TeamA:
		return Settings->LobbyTeamAColor;
	case EGameTeam::TeamB:
		return Settings->LobbyTeamBColor;
	default:
		// 미배정은 팀 색이 없음
		return FLinearColor::Gray;
	}
}
