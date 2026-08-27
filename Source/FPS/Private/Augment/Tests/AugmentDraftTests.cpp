#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentDraft.h"
#include "Tags/GameplayTags.h"

namespace
{
	FAugmentInventoryEntry MakeEntry(const int32 Index)
	{
		FAugmentInventoryEntry Entry;
		Entry.DefinitionId = FPrimaryAssetId(
			UAugmentDefinition::AugmentAssetType,
			FName(*FString::Printf(TEXT("DraftTest_%d"), Index)));
		Entry.LineTag = RogueTags::Augment_Line;
		return Entry;
	}

	TArray<FAugmentInventoryEntry> MakeBucket(const int32 Count)
	{
		TArray<FAugmentInventoryEntry> Bucket;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Bucket.Add(MakeEntry(Index));
		}
		return Bucket;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAugmentCompleteOfferChainsTest,
	"FPS.Augment.Draft.CompleteOfferChains",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAugmentCompleteOfferChainsTest::RunTest(const FString& Parameters)
{
	TArray<FAugmentInventoryEntry> Bucket = MakeBucket(6);
	FAugmentOfferResult Result;

	TestTrue(TEXT("Six candidates build three complete one-reroll slots"),
		RogueAugmentDraft::TryPickCompleteOffers(Bucket, 3, 731, 2, Result));
	TestEqual(TEXT("Three slots are produced"), Result.SlotCards.Num(), 3);
	TestEqual(TEXT("The complete draw consumes all six candidates"), Bucket.Num(), 0);

	TSet<FPrimaryAssetId> DrawnDefinitions;
	for (const TArray<FAugmentInventoryEntry>& Chain : Result.SlotCards)
	{
		TestEqual(TEXT("Each slot has one visible card and one reroll reserve"), Chain.Num(), 2);
		for (const FAugmentInventoryEntry& Entry : Chain)
		{
			DrawnDefinitions.Add(Entry.DefinitionId);
		}
	}
	TestEqual(TEXT("No card repeats within a selection session"), DrawnDefinitions.Num(), 6);

	TArray<FAugmentInventoryEntry> SecondSessionBucket = MakeBucket(6);
	FAugmentOfferResult SecondSession;
	TestTrue(TEXT("A second complete draw succeeds with the same input"),
		RogueAugmentDraft::TryPickCompleteOffers(SecondSessionBucket, 3, 731, 2, SecondSession));

	for (int32 SlotIndex = 0; SlotIndex < Result.SlotCards.Num(); ++SlotIndex)
	{
		for (int32 CardIndex = 0; CardIndex < Result.SlotCards[SlotIndex].Num(); ++CardIndex)
		{
			TestTrue(TEXT("The same seed produces the same session order"),
				Result.SlotCards[SlotIndex][CardIndex].DefinitionId
				== SecondSession.SlotCards[SlotIndex][CardIndex].DefinitionId);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAugmentIncompleteOfferSetTest,
	"FPS.Augment.Draft.InsufficientCandidatesFailAtomically",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAugmentIncompleteOfferSetTest::RunTest(const FString& Parameters)
{
	TArray<FAugmentInventoryEntry> Bucket = MakeBucket(5);
	const TArray<FAugmentInventoryEntry> OriginalBucket = Bucket;
	FAugmentOfferResult Result;
	TArray<FAugmentInventoryEntry> ExistingSlot;
	ExistingSlot.Add(MakeEntry(99));
	Result.SlotCards.Add(MoveTemp(ExistingSlot));
	Result.UnfilledSlots = 17;

	TestFalse(TEXT("Five candidates cannot build a complete three-slot one-reroll session"),
		RogueAugmentDraft::TryPickCompleteOffers(Bucket, 3, 731, 2, Result));
	TestEqual(TEXT("A failed complete draw does not consume the bucket"), Bucket.Num(), OriginalBucket.Num());
	TestEqual(TEXT("A failed complete draw preserves result metadata"), Result.UnfilledSlots, 17);
	TestEqual(TEXT("A failed complete draw preserves result slots"), Result.SlotCards.Num(), 1);
	TestEqual(TEXT("A failed complete draw preserves existing slot contents"), Result.SlotCards[0].Num(), 1);
	TestTrue(TEXT("A failed complete draw preserves the existing entry"),
		Result.SlotCards[0][0].DefinitionId == MakeEntry(99).DefinitionId);

	for (int32 Index = 0; Index < Bucket.Num(); ++Index)
	{
		TestTrue(TEXT("Candidate order is preserved after an atomic failure"),
			Bucket[Index].DefinitionId == OriginalBucket[Index].DefinitionId);
	}

	return true;
}

#endif
