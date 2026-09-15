#include "Training/TrainingAugmentRules.h"

#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentDraft.h"

#define LOCTEXT_NAMESPACE "TrainingAugmentRules"

bool TrainingAugmentRules::IsCatalogEligible(const UAugmentDefinition& Definition,
    const FGameplayTagContainer& UnlockTags)
{
    return Definition.AugmentCategory != EAugmentCategory::None
        && Definition.Tier < EAugmentTier::MAX
        && Definition.LineTag.IsValid() && Definition.GetLevelData()
        && RogueAugmentDraft::MeetsUnlockRequirements(Definition, UnlockTags);
}

TrainingAugmentRules::EToggleResult TrainingAugmentRules::Toggle(
    TArray<FPrimaryAssetId>& Selection, const FPrimaryAssetId& Id)
{
    if (!Id.IsValid()) { return EToggleResult::InvalidId; }
    if (Selection.RemoveSingle(Id) > 0) { return EToggleResult::Deselected; }
    if (Selection.Num() >= MaximumSelection) { return EToggleResult::LimitReached; }
    Selection.Add(Id);
    return EToggleResult::Selected;
}

bool TrainingAugmentRules::ValidateIds(const TArray<FPrimaryAssetId>& Selection, FText& OutFailure)
{
    OutFailure = FText::GetEmpty();
    if (Selection.Num() > MaximumSelection)
    {
        OutFailure = LOCTEXT("Limit", "Select up to 3 augments in total (active + passive).");
        return false;
    }
    TSet<FPrimaryAssetId> Seen;
    for (const FPrimaryAssetId& Id : Selection)
    {
        if (!Id.IsValid() || Seen.Contains(Id))
        {
            OutFailure = LOCTEXT("Invalid", "The selection contains an invalid or duplicate augment.");
            return false;
        }
        Seen.Add(Id);
    }
    return true;
}

FGameplayTag TrainingAugmentRules::GetActiveSlot(const int32 ActiveIndex)
{
    static const FName Names[] = {
        TEXT("InputTag.Ability.Slot1"), TEXT("InputTag.Ability.Slot2"), TEXT("InputTag.Ability.Slot3")
    };
    return ActiveIndex >= 0 && ActiveIndex < UE_ARRAY_COUNT(Names)
        ? FGameplayTag::RequestGameplayTag(Names[ActiveIndex], false) : FGameplayTag();
}

FText TrainingAugmentRules::GetSlotLabel(const int32 ActiveIndex)
{
    switch (ActiveIndex)
    {
    case 0: return LOCTEXT("Slot1", "SLOT 1 / Q");
    case 1: return LOCTEXT("Slot2", "SLOT 2 / E");
    case 2: return LOCTEXT("Slot3", "SLOT 3 / V");
    default: return FText::GetEmpty();
    }
}

#undef LOCTEXT_NAMESPACE
