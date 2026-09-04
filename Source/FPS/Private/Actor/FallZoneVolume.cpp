// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/FallZoneVolume.h"

#include "Character/ShooterCharacter.h"
#include "Components/ShapeComponent.h"
#include "Engine/World.h"
#include "GAS/Damage/RogueDamageSubsystem.h"
#include "Tags/GameplayTags.h"

AFallZoneVolume::AFallZoneVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	UShapeComponent* Trigger = GetCollisionComponent();
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->SetCanEverAffectNavigation(false);

	OnActorBeginOverlap.AddDynamic(this, &AFallZoneVolume::HandleBeginOverlap);
}

void AFallZoneVolume::HandleBeginOverlap(	AActor*,	AActor* OtherActor)
{
	if (GetNetMode() == NM_Client)
	{
		return;
	}

	AShooterCharacter* Character = Cast<AShooterCharacter>(OtherActor);
	if (!IsValid(Character))
	{
		return;
	}

	URogueDamageSubsystem* DamageSystem =
		GetWorld()->GetSubsystem<URogueDamageSubsystem>();

	if (!IsValid(DamageSystem))
	{
		return;
	}

	FRogueDamageParams DamageParams;
	DamageParams.Causer = this;
	DamageParams.SourceTag = RogueTags::Damage_Source_Environment;

	DamageSystem->ApplyFlatDamage(
		DamageParams,
		Character,
		DamageAmount);
}