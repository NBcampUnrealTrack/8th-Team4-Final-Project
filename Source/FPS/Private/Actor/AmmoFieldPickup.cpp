#include "Actor/AmmoFieldPickup.h"

#include "GAS/RogueAmmoPickupEffect.h"

AAmmoFieldPickup::AAmmoFieldPickup()
{
	PickupGameplayEffect = URogueAmmoPickupEffect::StaticClass();
}
