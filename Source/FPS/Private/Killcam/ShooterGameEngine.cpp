#include "Killcam/ShooterGameEngine.h"

bool UShooterGameEngine::Experimental_ShouldPreDuplicateMap(const FName MapName) const
{
	const FString MapString = MapName.ToString();
	return MapString.EndsWith(TEXT("BombMap")) || MapString.EndsWith(TEXT("DominationMap"));
}
