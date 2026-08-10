// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/RogueGASSettings.h"

URogueGASSettings::URogueGASSettings()
{
	TierDraftWeights.Add(EAugmentTier::Common,    { 65.f, -3.0f });
	TierDraftWeights.Add(EAugmentTier::Rare,      { 30.f,  1.8f });
	TierDraftWeights.Add(EAugmentTier::Epic,      {  5.f,  1.0f });
	TierDraftWeights.Add(EAugmentTier::Legendary, {  0.f,  0.2f });
}
