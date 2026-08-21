// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/AugmentInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Augment/RogueAugmentSettings.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogAugmentInventory, Log, All);

namespace
{
	// 부여 가능 여부만 확인
	bool CanApply(const UAugmentDefinition& Definition, int32 Level)
	{
		FAugmentResolvedGrant Unused;
		return RogueAugmentAssembler::ResolveAugment(Definition, Level, Unused);
	}

	bool IsGrantComplete(const UAugmentDefinition& Definition, const FAugmentGrantHandles& Handles)
	{
		return Handles.EffectHandles.Num() == Definition.GrantedEffects.Num()
			&& Handles.AbilityHandles.Num() == Definition.GrantedAbilities.Num();
	}

	void LogIncompleteGrant(
		const UAugmentDefinition& Definition,
		const int32 Level,
		const FAugmentGrantHandles& Handles)
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s %d레벨 부여가 일부 실패했습니다 (GE %d/%d, GA %d/%d)"),
			*Definition.GetName(), Level,
			Handles.EffectHandles.Num(), Definition.GrantedEffects.Num(),
			Handles.AbilityHandles.Num(), Definition.GrantedAbilities.Num());
	}
}

UAugmentInventoryComponent::UAugmentInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void UAugmentInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 소유자에게만. 남에게 보일 목록은 밖에서 스냅샷으로
	DOREPLIFETIME_CONDITION(UAugmentInventoryComponent, Entries, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAugmentInventoryComponent, Offers, COND_OwnerOnly);
}

void UAugmentInventoryComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (AbilitySystemComponent == InASC)
	{
		return;
	}

	// 이전 ASC에서 부여한 GE/GA 제거
	RemoveAllGrants();

	// ASC 교체
	AbilitySystemComponent = InASC;
}

void UAugmentInventoryComponent::UninitializeFromAbilitySystem()
{
	GrantHandlesByLine.Reset();
	AbilitySystemComponent = nullptr;
}

void UAugmentInventoryComponent::RemoveAllGrants()
{
	if (!IsValid(AbilitySystemComponent))
	{
		if (!GrantHandlesByLine.IsEmpty())
		{
			UE_LOG(LogAugmentInventory, Warning,
				TEXT("[AugmentInventory] %s: ASC 가 없어 %d개 계열의 효과를 걷어내지 못했습니다"),
				*GetNameSafe(GetOwner()), GrantHandlesByLine.Num());
		}

		GrantHandlesByLine.Reset();
		return;
	}

	for (TPair<FGameplayTag, FAugmentGrantHandles>& Pair : GrantHandlesByLine)
	{
		RogueAugmentAssembler::RemoveAugment(*AbilitySystemComponent, Pair.Value);
	}

	GrantHandlesByLine.Reset();
}

bool UAugmentInventoryComponent::ServerRollOffers(const FAugmentDraftContext& DraftContext, int32 CardCount)
{
	if (!HasServerAuthority(TEXT("추첨")))
	{
		return false;
	}

	const AActor* Owner = GetOwner();

	// 후보를 만드는 동안에는 선택 RPC를 닫아 둔다. 모든 플레이어의
	// 준비가 성공한 뒤 DraftManager가 한 번에 선택 가능 상태로 전환한다.
	bOfferSelectionEnabled = false;

	if (StagedOfferState != EAugmentOfferCommitState::Empty)
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s: 이전 라운드의 확정 대기 선택이 남아 있어 새 후보를 만들 수 없습니다"),
			*GetNameSafe(Owner));
		return false;
	}

	if (CardCount <= 0)
	{
		return false;
	}

	// 증강 후보 미등록 시점
	const UAugmentRegistrySubsystem* Registry = GetRegistry();
	if (!Registry || !Registry->IsReady())
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] 레지스트리가 준비되기 전에 추첨을 돌렸습니다. 선택 페이즈는 IsReady() 확인 후에 여세요"));
		return false;
	}

	// ASC 보유 여부 확인
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s: ASC 가 없어 추첨할 수 없습니다"), *GetNameSafe(Owner));
		return false;
	}

	// 후보가 남아 있는 경우
	if (!Offers.IsEmpty())
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s: 지난 라운드 후보 %d개가 남아 있습니다. ")
			TEXT("페이즈 종료 시 자동 선택 호출이 빠졌거나, 획득이 계속 실패했습니다"),
			*GetNameSafe(Owner), Offers.Num());
	}

	// 후보를 등급별로 나눠서 버킷화
	TArray<TArray<FAugmentInventoryEntry>> Buckets;
	GatherCandidatesByTier(Buckets);

	int32 CandidateCount = 0;
	FString BucketSummary;
	for (int32 TierIndex = 0; TierIndex < Buckets.Num(); ++TierIndex)
	{
		CandidateCount += Buckets[TierIndex].Num();

		BucketSummary += FString::Printf(TEXT("%s%s %d"),
			BucketSummary.IsEmpty() ? TEXT("") : TEXT(" / "),
			*UEnum::GetDisplayValueAsText(static_cast<EAugmentTier>(TierIndex)).ToString(),
			Buckets[TierIndex].Num());
	}

	// 후보가 없는 경우, AssetManager 스캔 경로가 잘못된 경우
	if (CandidateCount == 0)
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s: 후보가 0개입니다. AssetManager 스캔 경로와 PDA 검증을 확인하세요"),
			*GetNameSafe(Owner));
		return false;
	}

	const int32 Seed = MakeDraftSeed(DraftContext);
	FRandomStream Stream(Seed);

	// 자동 선택이 같은 시드에서 갈라져 나온다
	LastDraftSeed = Seed;

	// 시드를 남겨둠
	UE_LOG(LogAugmentInventory, Log,
		TEXT("[AugmentInventory] %s: %d라운드 추첨 시드 %d, 후보 %d개 (%s)"),
		*GetNameSafe(Owner), DraftContext.RoundNumber, Seed, CandidateCount, *BucketSummary);

	Offers.Reset();

	// 등급 후보가 비어 있거나 가중치가 0이면 뽑을 수 없음
	auto IsDrawable = [&](int32 TierIndex)
	{
		return !Buckets[TierIndex].IsEmpty()
			&& GetTierDraftWeight(static_cast<EAugmentTier>(TierIndex), DraftContext.RoundNumber) > 0.f;
	};

	for (int32 Card = 0; Card < CardCount; ++Card)
	{
		float TotalWeight = 0.f;
		for (int32 TierIndex = 0; TierIndex < Buckets.Num(); ++TierIndex)
		{
			if (IsDrawable(TierIndex))
			{
				TotalWeight += GetTierDraftWeight(static_cast<EAugmentTier>(TierIndex), DraftContext.RoundNumber);
			}
		}

		if (TotalWeight <= 0.f)
		{
			UE_LOG(LogAugmentInventory, Warning,
				TEXT("[AugmentInventory] %s: 뽑을 수 있는 등급이 남지 않아 %d장만 제시합니다"),
				*GetNameSafe(Owner), Offers.Num());
			break;
		}

		const float Roll = Stream.FRandRange(0.f, TotalWeight);

		int32 PickedTier  = INDEX_NONE;
		float Accumulated = 0.f;
		for (int32 TierIndex = 0; TierIndex < Buckets.Num(); ++TierIndex)
		{
			if (!IsDrawable(TierIndex))
			{
				continue;
			}

			PickedTier   = TierIndex;
			Accumulated += GetTierDraftWeight(static_cast<EAugmentTier>(TierIndex), DraftContext.RoundNumber);

			if (Roll <= Accumulated)
			{
				break;
			}
		}

		TArray<FAugmentInventoryEntry>& Bucket = Buckets[PickedTier];

		const int32 PickedIndex = Stream.RandRange(0, Bucket.Num() - 1);
		Offers.Add(Bucket[PickedIndex]);
		Bucket.RemoveAt(PickedIndex);
	}

	const bool bRolled = !Offers.IsEmpty();

	// 서버에는 OnRep 이 오지 않으므로 직접 발신
	OnOffersChanged.Broadcast();

	return bRolled;
}

void UAugmentInventoryComponent::ServerSelectOffer_Implementation(int32 OfferIndex)
{
	if (!bOfferSelectionEnabled)
	{
		UE_LOG(LogAugmentInventory, Warning,
			TEXT("[AugmentInventory] %s: 선택 페이즈가 닫힌 상태에서 후보 선택 요청을 거부했습니다"),
			*GetNameSafe(GetOwner()));
		return;
	}

	StageOfferSelection(OfferIndex, TEXT("클라이언트 선택"));
}

bool UAugmentInventoryComponent::ServerSetOfferSelectionEnabled(const bool bEnabled)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	bOfferSelectionEnabled = bEnabled && !Offers.IsEmpty() && StagedOfferState == EAugmentOfferCommitState::Empty;
	return !bEnabled || bOfferSelectionEnabled;
}

bool UAugmentInventoryComponent::ServerAutoSelectOffer()
{
	if (!HasServerAuthority(TEXT("자동 선택")))
	{
		return false;
	}

	const AActor* Owner = GetOwner();

	bOfferSelectionEnabled = false;
	if (StagedOfferState != EAugmentOfferCommitState::Empty)
	{
		return true;
	}

	if (Offers.IsEmpty())
	{
		return false;
	}

	const uint32 AutoSelectSeed = HashCombine(static_cast<uint32>(LastDraftSeed), 0x9E3779B9u);
	FRandomStream Stream(static_cast<int32>(AutoSelectSeed));
	const int32 PickedIndex = Stream.RandRange(0, Offers.Num() - 1);

	UE_LOG(LogAugmentInventory, Log,
		TEXT("[AugmentInventory] %s: 자동 선택 %d번 (후보 %d장, 시드 %d)"),
		*GetNameSafe(Owner), PickedIndex, Offers.Num(), static_cast<int32>(AutoSelectSeed));

	return StageOfferSelection(PickedIndex, TEXT("자동 선택"));
}

bool UAugmentInventoryComponent::CanCommitStagedOffer() const
{
	const AActor* Owner = GetOwner();
	if (!Owner
		|| !Owner->HasAuthority()
		|| StagedOfferState != EAugmentOfferCommitState::Selected
		|| !IsValid(AbilitySystemComponent))
	{
		return false;
	}

	UAugmentDefinition* Definition = ResolveDefinition(StagedOffer.DefinitionId);
	return IsValid(Definition)
		&& Definition->LineTag == StagedOffer.LineTag
		&& Definition->HasLevel(StagedOffer.Level)
		&& StagedOffer.Level == GetLevelInLine(StagedOffer.LineTag) + 1
		&& CanApply(*Definition, StagedOffer.Level);
}

bool UAugmentInventoryComponent::CommitStagedOffer()
{
	if (!CanCommitStagedOffer())
	{
		return false;
	}

	UAugmentDefinition* Definition = ResolveDefinition(StagedOffer.DefinitionId);
	StagedPreviousLine = CaptureLineSnapshot(StagedOffer.LineTag);
	StagedOfferState = EAugmentOfferCommitState::RecoveryPending;

	if (!IsValid(Definition)
		|| !ApplyLineAugment(*Definition, StagedOffer.Level, StagedPreviousLine.EntryIndex))
	{
		const bool bRestored = RestoreLineSnapshot(StagedPreviousLine);
		RebuildConditionalRuleCache();
		OnInventoryChanged.Broadcast();

		if (bRestored)
		{
			StagedPreviousLine.Reset();
			StagedOfferState = EAugmentOfferCommitState::Selected;
		}
		return false;
	}

	RebuildConditionalRuleCache();
	OnInventoryChanged.Broadcast();
	StagedOfferState = EAugmentOfferCommitState::Applied;
	return true;
}

void UAugmentInventoryComponent::FinalizeStagedOfferCommit()
{
	if (StagedOfferState == EAugmentOfferCommitState::Applied)
	{
		ClearStagedOffer();
	}
}

bool UAugmentInventoryComponent::RollbackStagedOfferCommit()
{
	if (StagedOfferState == EAugmentOfferCommitState::Empty)
	{
		return true;
	}

	if (StagedOfferState == EAugmentOfferCommitState::Selected)
	{
		ClearStagedOffer();
		return true;
	}

	const bool bRestored = RestoreLineSnapshot(StagedPreviousLine);
	RebuildConditionalRuleCache();
	OnInventoryChanged.Broadcast();
	if (bRestored)
	{
		ClearStagedOffer();
	}
	else
	{
		StagedOfferState = EAugmentOfferCommitState::RecoveryPending;
	}
	return bRestored;
}

bool UAugmentInventoryComponent::StageOfferSelection(int32 OfferIndex, const TCHAR* Reason)
{
	const AActor* Owner = GetOwner();
	if (StagedOfferState != EAugmentOfferCommitState::Empty)
	{
		UE_LOG(LogAugmentInventory, Warning,
			TEXT("[AugmentInventory] %s: 이미 선택을 완료한 상태에서 %s 요청이 들어왔습니다"),
			*GetNameSafe(Owner), Reason);
		return false;
	}

	if (!Offers.IsValidIndex(OfferIndex))
	{
		UE_LOG(LogAugmentInventory, Warning,
			TEXT("[AugmentInventory] %s: %s 인덱스 %d가 후보 %d개 범위 밖입니다"),
			*GetNameSafe(Owner), Reason, OfferIndex, Offers.Num());
		return false;
	}

	StagedOffer = Offers[OfferIndex];
	StagedOfferState = EAugmentOfferCommitState::Selected;
	if (!CanCommitStagedOffer())
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s: 선택한 후보 %s를 확정할 수 없어 요청을 거부했습니다"),
			*GetNameSafe(Owner), *StagedOffer.DefinitionId.ToString());
		ClearStagedOffer();
		return false;
	}

	bOfferSelectionEnabled = false;
	Offers.Reset();
	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->SetLooseGameplayTagCount(
			RogueTags::State_Gameplay_InputBlocked_AugmentDraft,
			0,
			EGameplayTagReplicationState::TagOnly);
	}
	OnOffersChanged.Broadcast();
	return true;
}

void UAugmentInventoryComponent::ClearStagedOffer()
{
	StagedOffer = FAugmentInventoryEntry();
	StagedPreviousLine.Reset();
	StagedOfferState = EAugmentOfferCommitState::Empty;
}

void UAugmentInventoryComponent::ServerClearOffers()
{
	if (!HasServerAuthority(TEXT("후보 폐기")))
	{
		return;
	}

	const AActor* Owner = GetOwner();

	const bool bHadPendingDraftState = !Offers.IsEmpty()
		|| StagedOfferState != EAugmentOfferCommitState::Empty;
	bOfferSelectionEnabled = false;
	Offers.Reset();

	if (StagedOfferState == EAugmentOfferCommitState::Applied
		|| StagedOfferState == EAugmentOfferCommitState::RecoveryPending)
	{
		if (!RollbackStagedOfferCommit())
		{
			UE_LOG(LogAugmentInventory, Error,
				TEXT("[AugmentInventory] %s: 취소된 드래프트 보상 롤백에 실패했습니다"),
				*GetNameSafe(Owner));
		}
	}
	else
	{
		ClearStagedOffer();
	}

	if (bHadPendingDraftState)
	{
		OnOffersChanged.Broadcast();
	}
}

void UAugmentInventoryComponent::ServerRemoveAllAugments()
{
	if (!HasServerAuthority(TEXT("증강 전체 해제")))
	{
		return;
	}

	bOfferSelectionEnabled = false;
	ClearStagedOffer();
	RemoveAllGrants();

	Entries.Reset();
	CachedConditionalRules.Reset();

	// 선택되지 않은 후보 제거
	Offers.Reset();

	OnInventoryChanged.Broadcast();
	OnOffersChanged.Broadcast();
}

int32 UAugmentInventoryComponent::GetLevelInLine(FGameplayTag LineTag) const
{
	const int32 Index = FindEntryIndexInLine(LineTag);

	// 엔트리가 레벨을 의미함
	return Index == INDEX_NONE ? 0 : Entries[Index].Level;
}

UAugmentInventoryComponent* UAugmentInventoryComponent::FindInventory(const UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return nullptr;
	}

	// PlayerState 에 부착하는 것이 기본
	if (const AActor* OwnerActor = ASC->GetOwnerActor())
	{
		if (UAugmentInventoryComponent* Found = OwnerActor->FindComponentByClass<UAugmentInventoryComponent>())
		{
			return Found;
		}
	}

	// PlayerState가 아닌 경우 아바타를 확인
	if (const AActor* AvatarActor = ASC->GetAvatarActor())
	{
		return AvatarActor->FindComponentByClass<UAugmentInventoryComponent>();
	}

	return nullptr;
}

void UAugmentInventoryComponent::OnUnregister()
{
	UninitializeFromAbilitySystem();

	Super::OnUnregister();
}

void UAugmentInventoryComponent::OnRep_Entries()
{
	OnInventoryChanged.Broadcast();
}

void UAugmentInventoryComponent::OnRep_Offers()
{
	OnOffersChanged.Broadcast();
}

UAugmentRegistrySubsystem* UAugmentInventoryComponent::GetRegistry() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

	return GameInstance ? GameInstance->GetSubsystem<UAugmentRegistrySubsystem>() : nullptr;
}

UAugmentDefinition* UAugmentInventoryComponent::ResolveDefinition(const FPrimaryAssetId& DefinitionId) const
{
	const UAugmentRegistrySubsystem* Registry = GetRegistry();

	return Registry ? Registry->FindDefinition(DefinitionId) : nullptr;
}

bool UAugmentInventoryComponent::HasServerAuthority(const TCHAR* Context) const
{
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		return true;
	}

	UE_LOG(LogAugmentInventory, Error,
		TEXT("[AugmentInventory] %s: 권한이 없는 곳에서 %s 이(가) 호출됐습니다"),
		*GetNameSafe(Owner), Context);

	return false;
}

int32 UAugmentInventoryComponent::FindEntryIndexInLine(const FGameplayTag& LineTag) const
{
	return Entries.IndexOfByPredicate(
		[&LineTag](const FAugmentInventoryEntry& Entry) { return Entry.LineTag.MatchesTagExact(LineTag); });
}

FAugmentLineSnapshot UAugmentInventoryComponent::CaptureLineSnapshot(const FGameplayTag& LineTag) const
{
	FAugmentLineSnapshot Snapshot;
	Snapshot.LineTag = LineTag;
	Snapshot.EntryIndex = FindEntryIndexInLine(LineTag);
	if (Snapshot.EntryIndex != INDEX_NONE)
	{
		Snapshot.Entry = Entries[Snapshot.EntryIndex];
	}
	return Snapshot;
}

void UAugmentInventoryComponent::RemoveLineAugment(const FGameplayTag& LineTag)
{
	if (!IsValid(AbilitySystemComponent) || !LineTag.IsValid())
	{
		return;
	}

	if (FAugmentGrantHandles* Handles = GrantHandlesByLine.Find(LineTag))
	{
		RogueAugmentAssembler::RemoveAugment(*AbilitySystemComponent, *Handles);
		GrantHandlesByLine.Remove(LineTag);
	}

	const int32 EntryIndex = FindEntryIndexInLine(LineTag);
	if (EntryIndex != INDEX_NONE)
	{
		Entries.RemoveAt(EntryIndex);
	}
}

bool UAugmentInventoryComponent::ApplyLineAugment(
	UAugmentDefinition& Definition,
	const int32 Level,
	const int32 EntryIndex)
{
	if (!IsValid(AbilitySystemComponent) || !Definition.LineTag.IsValid() || !Definition.HasLevel(Level))
	{
		return false;
	}

	RemoveLineAugment(Definition.LineTag);

	FAugmentResolvedGrant Resolved;
	FAugmentGrantHandles NewHandles;
	if (RogueAugmentAssembler::ResolveAugment(Definition, Level, Resolved))
	{
		NewHandles = RogueAugmentAssembler::ApplyResolved(*AbilitySystemComponent, Resolved);
	}

	if (!IsGrantComplete(Definition, NewHandles))
	{
		LogIncompleteGrant(Definition, Level, NewHandles);
		RogueAugmentAssembler::RemoveAugment(*AbilitySystemComponent, NewHandles);
		return false;
	}

	GrantHandlesByLine.Add(Definition.LineTag, MoveTemp(NewHandles));
	const FAugmentInventoryEntry NewEntry = FAugmentInventoryEntry::Make(Definition, Level);
	if (EntryIndex == INDEX_NONE)
	{
		Entries.Add(NewEntry);
	}
	else
	{
		Entries.Insert(NewEntry, FMath::Clamp(EntryIndex, 0, Entries.Num()));
	}
	return true;
}

bool UAugmentInventoryComponent::RestoreLineSnapshot(const FAugmentLineSnapshot& Snapshot)
{
	if (!IsValid(AbilitySystemComponent) || !Snapshot.LineTag.IsValid())
	{
		return false;
	}

	RemoveLineAugment(Snapshot.LineTag);
	if (!Snapshot.Entry.IsSet())
	{
		return true;
	}

	const FAugmentInventoryEntry& PreviousEntry = Snapshot.Entry.GetValue();
	UAugmentDefinition* PreviousDefinition = ResolveDefinition(PreviousEntry.DefinitionId);
	if (!PreviousEntry.LineTag.MatchesTagExact(Snapshot.LineTag)
		|| !IsValid(PreviousDefinition)
		|| PreviousDefinition->LineTag != PreviousEntry.LineTag
		|| !PreviousDefinition->HasLevel(PreviousEntry.Level)
		|| !CanApply(*PreviousDefinition, PreviousEntry.Level))
	{
		return false;
	}

	return ApplyLineAugment(*PreviousDefinition, PreviousEntry.Level, Snapshot.EntryIndex);
}

void UAugmentInventoryComponent::GatherCandidatesByTier(TArray<TArray<FAugmentInventoryEntry>>& OutBuckets) const
{
	const int32 TierCount = static_cast<int32>(EAugmentTier::MAX);

	OutBuckets.Reset();
	OutBuckets.SetNum(TierCount);

	const UAugmentRegistrySubsystem* Registry = GetRegistry();
	if (!Registry)
	{
		return;
	}

	for (UAugmentDefinition* Definition : Registry->GetAllDefinitions())
	{
		if (!Definition || !Definition->LineTag.IsValid())
		{
			continue;
		}

		// 보유 레벨 + 1. 미보유면 1레벨이 후보
		const int32 NextLevel = GetLevelInLine(Definition->LineTag) + 1;

		// 배열에 그 레벨이 없으면 최고 레벨까지 올린 계열
		if (!Definition->HasLevel(NextLevel))
		{
			continue;
		}

		// 무기를 아직 못 얻었으면 그 무기 전용 계열은 후보에서 빠짐
		if (!Definition->RequiredUnlockTags.IsEmpty()
			&& (!AbilitySystemComponent
				|| !AbilitySystemComponent->HasAllMatchingGameplayTags(Definition->RequiredUnlockTags)))
		{
			continue;
		}

		const int32 TierIndex = static_cast<int32>(Definition->Tier);
		if (!OutBuckets.IsValidIndex(TierIndex))
		{
			UE_LOG(LogAugmentInventory, Error,
				TEXT("[AugmentInventory] %s 의 등급이 범위를 벗어났습니다. 후보에서 제외합니다"),
				*Definition->GetName());
			continue;
		}

		OutBuckets[TierIndex].Add(FAugmentInventoryEntry::Make(*Definition, NextLevel));
	}

	// 후보가 없는 등급이 있으면 경고. 그 등급의 확률은 다른 등급으로 재분배됨
	const URogueAugmentSettings* Settings = GetDefault<URogueAugmentSettings>();

	bool bWarnedEmptyTier = false;

	for (int32 TierIndex = 0; TierIndex < TierCount && !bWarnedEmptyTier; ++TierIndex)
	{
		const EAugmentTier Tier = static_cast<EAugmentTier>(TierIndex);

		const FAugmentTierDraftWeight* Weight = Settings ? Settings->TierDraftWeights.Find(Tier) : nullptr;
		const bool bEverDraftable = Weight && (Weight->BaseWeight > 0.f || Weight->PerRoundDelta > 0.f);

		if (OutBuckets[TierIndex].IsEmpty() && bEverDraftable)
		{
			UE_LOG(LogAugmentInventory, Warning,
				TEXT("[AugmentInventory] 후보가 없는 등급이 있습니다(%s 등). 그 등급의 확률은 다른 등급으로 재분배됩니다"),
				*UEnum::GetValueAsString(Tier));

			bWarnedEmptyTier = true;
		}
	}
}

float UAugmentInventoryComponent::GetTierDraftWeight(EAugmentTier Tier, int32 RoundNumber)
{
	const URogueAugmentSettings* Settings = GetDefault<URogueAugmentSettings>();
	const FAugmentTierDraftWeight* Weight = Settings ? Settings->TierDraftWeights.Find(Tier) : nullptr;

	if (!Weight)
	{
		return 0.f;
	}

	const float Rounds = static_cast<float>(FMath::Max(RoundNumber, 1) - 1);

	// 음수 가중치 방지
	return FMath::Max(Weight->BaseWeight + Weight->PerRoundDelta * Rounds, 0.f);
}

int32 UAugmentInventoryComponent::MakeDraftSeed(const FAugmentDraftContext& DraftContext) const
{
	// 시드가 없으면 랜덤
	if (DraftContext.RoundSeed == 0)
	{
		return FMath::Rand();
	}

	// 플레이어별로 다른 시드를 만들기 위해 PlayerState 의 PlayerId 를 섞음
	int32 PlayerKey = 0;
	if (const APlayerState* PlayerState = Cast<APlayerState>(GetOwner()))
	{
		PlayerKey = PlayerState->GetPlayerId();
	}
	else if (const AActor* Owner = GetOwner())
	{
		PlayerKey = GetTypeHash(Owner->GetFName());
	}

	const uint32 WithPlayer = HashCombine(static_cast<uint32>(DraftContext.RoundSeed), static_cast<uint32>(PlayerKey));

	return static_cast<int32>(HashCombine(WithPlayer, static_cast<uint32>(DraftContext.RoundNumber)));
}

void UAugmentInventoryComponent::RebuildConditionalRuleCache()
{
	CachedConditionalRules.Reset();

	for (const FAugmentInventoryEntry& Entry : Entries)
	{
		const UAugmentDefinition* Definition = ResolveDefinition(Entry.DefinitionId);
		if (!Definition)
		{
			continue;
		}

		// 규칙은 레벨마다 다름
		if (const FAugmentLevelData* LevelData = Definition->FindLevel(Entry.Level))
		{
			CachedConditionalRules.Append(LevelData->ConditionalDamageRules);
		}
	}
}
