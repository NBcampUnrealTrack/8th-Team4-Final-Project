// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/AugmentInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentDraft.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Augment/RogueAugmentStatResolver.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogAugmentInventory, Log, All);

namespace
{
	// 부여 가능 여부만 확인
	bool CanApply(const UAugmentDefinition& Definition)
	{
		FAugmentResolvedGrant Unused;
		return RogueAugmentAssembler::ResolveAugment(Definition, Unused);
	}

	bool IsGrantComplete(const UAugmentDefinition& Definition, const FAugmentGrantHandles& Handles)
	{
		return Handles.EffectHandles.Num() == Definition.GrantedEffects.Num()
			&& Handles.AbilityHandles.Num() == Definition.GrantedAbilities.Num();
	}

	void LogIncompleteGrant(
		const UAugmentDefinition& Definition,
		const FAugmentGrantHandles& Handles)
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s 부여가 일부 실패했습니다 (GE %d/%d, GA %d/%d)"),
			*Definition.GetName(),
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

	// UI 표시를 위한 슬롯별 리롤 횟수
	DOREPLIFETIME_CONDITION(UAugmentInventoryComponent, SlotRerollsRemaining, COND_OwnerOnly);
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

bool UAugmentInventoryComponent::SetOffers(
	const TArray<TArray<FAugmentInventoryEntry>>& SlotCards,
	const int32 DraftSeed)
{
	if (!HasServerAuthority(TEXT("후보 설정")))
	{
		return false;
	}

	const AActor* Owner = GetOwner();

	// 후보를 채우는 동안에는 선택 RPC를 비활성화
	bOfferSelectionEnabled = false;

	if (StagedOfferState != EAugmentOfferCommitState::Empty)
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s: 이전 라운드의 확정 대기 선택이 남아 있어 새 후보를 채울 수 없습니다"),
			*GetNameSafe(Owner));
		return false;
	}

	if (SlotCards.IsEmpty())
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s: 빈 후보 목록을 받았습니다"), *GetNameSafe(Owner));
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

	ClearRoundDraftState();

	// 각 체인의 첫 장이 이번에 제시할 후보, 나머지는 리롤 예비
	Offers.Reserve(SlotCards.Num());
	SlotReserves.Reserve(SlotCards.Num());
	SlotRerollsRemaining.Reserve(SlotCards.Num());

	// 가장 앞에 있는 후보를 제시하고 나머지를 후보에 저장
	for (const TArray<FAugmentInventoryEntry>& Chain : SlotCards)
	{
		Offers.Add(Chain[0]);

		TArray<FAugmentInventoryEntry>& Reserve = SlotReserves.AddDefaulted_GetRef();
		Reserve.Append(Chain.GetData() + 1, Chain.Num() - 1);

		SlotRerollsRemaining.Add(Reserve.Num());
	}

	// 자동 선택에서 사용할 시드
	LastDraftSeed = DraftSeed;

	UE_LOG(LogAugmentInventory, Log,
		TEXT("[AugmentInventory] %s: 후보 %d장 수신 (시드 %d, 슬롯별 리롤 %s)"),
		*GetNameSafe(Owner), Offers.Num(), DraftSeed,
		*FString::JoinBy(SlotRerollsRemaining, TEXT("/"), [](const int32 Count) { return FString::FromInt(Count); }));

	// 서버에는 OnRep 이 오지 않으므로 직접 발신
	OnOffersChanged.Broadcast();

	return true;
}

void UAugmentInventoryComponent::FillDraftEligibility(FAugmentDraftEligibility& OutState) const
{
	OutState.Owned = Entries;

	// 액티브 보유 수와 이미 쓰고 있는 슬롯
	OutState.OccupiedActiveSlots.Reset();
	OutState.OwnedActiveCount = 0;
	for (const FAugmentInventoryEntry& Entry : Entries)
	{
		const UAugmentDefinition* Definition = ResolveDefinition(Entry.DefinitionId);
		if (!Definition || Definition->AugmentCategory != EAugmentCategory::Active)
		{
			continue;
		}

		++OutState.OwnedActiveCount;

		const FGameplayTag SlotTag = Definition->GetActiveInputSlotTag();
		if (SlotTag.IsValid())
		{
			OutState.OccupiedActiveSlots.AddTag(SlotTag);
		}
	}

	// 무기 보유 조건 태그
	OutState.UnlockTags.Reset();
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetOwnedGameplayTags(OutState.UnlockTags);
	}

	// 이미 등장한 후보는 다시 나오지 않음
	OutState.SeenDefinitions = SeenDefinitions;

	// 제시할 1장 + 리롤 몫(기본 1회 + 리롤 횟수 증강)
	OutState.CardsPerSlot = 2 + URogueAugmentStatResolver::GetBonusRerollCount(GetOwner());
}

void UAugmentInventoryComponent::MarkOffersSeen(const TArray<FAugmentInventoryEntry>& InOffers)
{
	if (!HasServerAuthority(TEXT("등장 이력 기록")))
	{
		return;
	}

	for (const FAugmentInventoryEntry& Entry : InOffers)
	{
		SeenDefinitions.Add(Entry.DefinitionId);
	}
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

	if (!StageOfferSelection(OfferIndex, TEXT("클라이언트 선택")))
	{
		return;
	}

	// 준비 페이즈에서도 실제 GAS 상태를 보여주되 Finalize 전까지는 롤백 가능한 상태로 둔다.
	if (!CommitStagedOffer())
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s: 선택한 증강의 즉시 적용에 실패했습니다. 확정 단계에서 다시 검증합니다"),
			*GetNameSafe(GetOwner()));
	}
}

// 0보다 크면 UI의 버튼 활성화
int32 UAugmentInventoryComponent::GetSlotRerollsRemaining(const int32 OfferIndex) const
{
	return SlotRerollsRemaining.IsValidIndex(OfferIndex) ? SlotRerollsRemaining[OfferIndex] : 0;
}

void UAugmentInventoryComponent::ServerRerollOffer_Implementation(const int32 OfferIndex)
{
	const AActor* Owner = GetOwner();

	// 선택 페이즈가 닫힌 경우
	if (!bOfferSelectionEnabled)
	{
		UE_LOG(LogAugmentInventory, Warning,
			TEXT("[AugmentInventory] %s: 선택 페이즈가 닫힌 상태에서 리롤 요청을 거부했습니다"),
			*GetNameSafe(Owner));
		return;
	}

	// 이미 카드를 고른 경우
	if (StagedOfferState != EAugmentOfferCommitState::Empty)
	{
		UE_LOG(LogAugmentInventory, Warning,
			TEXT("[AugmentInventory] %s: 선택을 완료한 상태에서 리롤 요청이 들어왔습니다"),
			*GetNameSafe(Owner));
		return;
	}

	// 후보 인덱스가 범위를 벗어난 경우
	if (!Offers.IsValidIndex(OfferIndex) || !SlotReserves.IsValidIndex(OfferIndex))
	{
		UE_LOG(LogAugmentInventory, Warning,
			TEXT("[AugmentInventory] %s: 리롤 인덱스 %d가 후보 %d개 범위 밖입니다"),
			*GetNameSafe(Owner), OfferIndex, Offers.Num());
		return;
	}

	// 예비 슬롯이 비어 있는 경우
	TArray<FAugmentInventoryEntry>& Reserve = SlotReserves[OfferIndex];
	if (Reserve.IsEmpty())
	{
		UE_LOG(LogAugmentInventory, Warning,
			TEXT("[AugmentInventory] %s: %d번 슬롯의 리롤 횟수가 남아 있지 않습니다"),
			*GetNameSafe(Owner), OfferIndex);
		return;
	}

	// 예비에서 다음 장을 꺼내 슬롯을 교체
	const FAugmentInventoryEntry NewOffer = Reserve[0];
	Reserve.RemoveAt(0);
	SlotRerollsRemaining[OfferIndex] = Reserve.Num();

	UE_LOG(LogAugmentInventory, Log,
		TEXT("[AugmentInventory] %s: %d번 슬롯 리롤 %s -> %s (잔여 %d회)"),
		*GetNameSafe(Owner), OfferIndex,
		*Offers[OfferIndex].DefinitionId.ToString(), *NewOffer.DefinitionId.ToString(), Reserve.Num());

	Offers[OfferIndex] = NewOffer;

	// 새로운 후보를 등장 이력에 기록. 레벨업 후보는 제외
	MarkOffersSeen({ NewOffer });

	// 서버에는 OnRep 이 오지 않으므로 직접 발신
	OnOffersChanged.Broadcast();
}

bool UAugmentInventoryComponent::SetOfferSelectionEnabled(const bool bEnabled)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}

	bOfferSelectionEnabled = bEnabled && !Offers.IsEmpty() && StagedOfferState == EAugmentOfferCommitState::Empty;
	return !bEnabled || bOfferSelectionEnabled;
}

bool UAugmentInventoryComponent::AutoSelectOffer()
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
	if (!Owner || !Owner->HasAuthority() || !IsValid(AbilitySystemComponent))
	{
		return false;
	}

	// 직접 선택은 이미 적용됐을 수 있다. 최종 확정 전의 Applied도 커밋 배리어를 충족한 상태다.
	if (StagedOfferState == EAugmentOfferCommitState::Applied)
	{
		return true;
	}

	if (StagedOfferState != EAugmentOfferCommitState::Selected)
	{
		return false;
	}

	UAugmentDefinition* Definition = ResolveDefinition(StagedOffer.DefinitionId);
	return IsValid(Definition)
		&& Definition->LineTag == StagedOffer.LineTag
		&& FindEntryIndexInLine(StagedOffer.LineTag) == INDEX_NONE
		&& CanApply(*Definition);
}

bool UAugmentInventoryComponent::CommitStagedOffer()
{
	if (!CanCommitStagedOffer())
	{
		return false;
	}

	// 이미 적용된 Pending 보상은 다시 적용하지 않고 성공으로 처리한다.
	if (StagedOfferState == EAugmentOfferCommitState::Applied)
	{
		return true;
	}

	UAugmentDefinition* Definition = ResolveDefinition(StagedOffer.DefinitionId);
	if (!IsValid(Definition))
	{
		return false;
	}

	if (Definition->AugmentCategory == EAugmentCategory::Active
		&& !Definition->GetActiveInputSlotTag().IsValid())
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s: %s 에 입력 슬롯 태그가 없어 부여하지 않습니다"),
			*GetNameSafe(GetOwner()), *Definition->LineTag.ToString());
		return false;
	}

	StagedPreviousLine = CaptureLineSnapshot(StagedOffer.LineTag);
	StagedOfferState = EAugmentOfferCommitState::RecoveryPending;

	// 선택한 증강 부여
	const bool bApplied = ApplyLineAugment(*Definition, StagedPreviousLine.EntryIndex);

	if (!bApplied)
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

	// 증강을 이전 상태로 되돌림
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

	ClearRoundDraftState();
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

void UAugmentInventoryComponent::ClearRoundDraftState()
{
	bOfferSelectionEnabled = false;

	Offers.Reset();
	SlotReserves.Reset();
	SlotRerollsRemaining.Reset();
}

void UAugmentInventoryComponent::ClearOffers()
{
	if (!HasServerAuthority(TEXT("후보 폐기")))
	{
		return;
	}

	const AActor* Owner = GetOwner();

	const bool bHadPendingDraftState = !Offers.IsEmpty()
		|| StagedOfferState != EAugmentOfferCommitState::Empty;

	ClearRoundDraftState();

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

void UAugmentInventoryComponent::RemoveAllAugments()
{
	if (!HasServerAuthority(TEXT("증강 전체 해제")))
	{
		return;
	}

	ClearStagedOffer();
	RemoveAllGrants();

	Entries.Reset();
	CachedConditionalRules.Reset();
	ClearRoundDraftState();

	// 등장 이력 초기화
	SeenDefinitions.Reset();

	OnInventoryChanged.Broadcast();
	OnOffersChanged.Broadcast();
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
	const int32 EntryIndex)
{
	if (!IsValid(AbilitySystemComponent) || !Definition.LineTag.IsValid())
	{
		return false;
	}

	RemoveLineAugment(Definition.LineTag);

	FAugmentResolvedGrant Resolved;
	FAugmentGrantHandles NewHandles;
	if (RogueAugmentAssembler::ResolveAugment(Definition, Resolved))
	{
		NewHandles = RogueAugmentAssembler::ApplyResolved(*AbilitySystemComponent, Resolved);
	}

	if (!IsGrantComplete(Definition, NewHandles))
	{
		LogIncompleteGrant(Definition, NewHandles);
		RogueAugmentAssembler::RemoveAugment(*AbilitySystemComponent, NewHandles);
		return false;
	}

	GrantHandlesByLine.Add(Definition.LineTag, MoveTemp(NewHandles));
	const FAugmentInventoryEntry NewEntry = FAugmentInventoryEntry::Make(Definition);
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
		|| !CanApply(*PreviousDefinition))
	{
		return false;
	}

	return ApplyLineAugment(*PreviousDefinition, Snapshot.EntryIndex);
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

		if (const FAugmentLevelData* LevelData = Definition->GetLevelData())
		{
			CachedConditionalRules.Append(LevelData->ConditionalDamageRules);
		}
	}
}
