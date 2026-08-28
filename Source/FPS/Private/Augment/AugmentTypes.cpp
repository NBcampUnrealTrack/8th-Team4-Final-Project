// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/AugmentTypes.h"

#include "Augment/AugmentDefinition.h"

FAugmentInventoryEntry FAugmentInventoryEntry::Make(const UAugmentDefinition& Definition)
{
	FAugmentInventoryEntry Entry;
	Entry.DefinitionId = Definition.GetPrimaryAssetId();
	Entry.LineTag      = Definition.LineTag;

	return Entry;
}
