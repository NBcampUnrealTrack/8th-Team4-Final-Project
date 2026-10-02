#include "Player/ShooterSpectatorPawn.h"

AShooterSpectatorPawn::AShooterSpectatorPawn()
{
	// Look input is routed by AShooterPlayerController so it uses the same
	// sensitivity and invert-Y setting as a normal player character.
	bAddDefaultMovementBindings = false;
}
