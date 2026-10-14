#include "GameMode/Settings/DominationSandstormSettings.h"

FVector UDominationSandstormSettings::GetNormalizedWindDirection() const
{
	const FVector Direction = WindDirection.GetSafeNormal();
	return Direction.ContainsNaN() || Direction.IsNearlyZero()
		? FVector::ForwardVector
		: Direction;
}
