// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/AugmentTypes.h"

#include "Augment/AugmentDefinition.h"

FAugmentInventoryEntry FAugmentInventoryEntry::Make(const UAugmentDefinition& Definition)
{
	FAugmentInventoryEntry Entry;
	Entry.DefinitionId = Definition.GetPrimaryAssetId();
	Entry.LineTag      = Definition.LineTag;
    if (Definition.AugmentCategory == EAugmentCategory::Active)
    {
        Entry.ActiveInputSlot = Definition.GetActiveInputSlotTag();
    }

	return Entry;
}
