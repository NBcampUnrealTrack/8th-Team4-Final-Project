// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/Damage/RogueDamageTypes.h"
#include "UObject/Interface.h"

#include "RogueDamageableTarget.generated.h"

class UCombatHitZoneDefinition;

UINTERFACE(MinimalAPI, BlueprintType, Blueprintable)
class URogueDamageableTarget : public UInterface
{
	GENERATED_BODY()
};

class FPS_API IRogueDamageableTarget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Rogue|Damage")
	void HandleDeath(const FRogueDeathContext& Context);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Rogue|Damage")
	UCombatHitZoneDefinition* GetCombatHitZoneDefinition() const;
};
