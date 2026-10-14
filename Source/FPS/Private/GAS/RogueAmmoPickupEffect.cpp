#include "GAS/RogueAmmoPickupEffect.h"

#include "GAS/RogueAmmoAttributeSet.h"
#include "GAS/RogueAmmoPickupRequirement.h"
#include "GameplayEffectComponents/CustomCanApplyGameplayEffectComponent.h"
#include "Tags/GameplayTags.h"

URogueAmmoPickupEffect::URogueAmmoPickupEffect(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat Amount;
	Amount.DataTag = RogueTags::Data_Magnitude_Primary;

	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = URogueAmmoAttributeSet::GetIncomingReserveAmmoAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Amount);

	UCustomCanApplyGameplayEffectComponent* Requirements =
		CreateDefaultSubobject<UCustomCanApplyGameplayEffectComponent>(TEXT("AmmoPickupRequirements"));
	Requirements->ApplicationRequirements.Add(URogueAmmoPickupRequirement::StaticClass());
	GEComponents.Add(Requirements);
}
