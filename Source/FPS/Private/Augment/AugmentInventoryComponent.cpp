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

DEFINE_LOG_CATEGORY_STATIC(LogAugmentInventory, Log, All);

namespace
{
	// 부여 결과 확인
	void VerifyGrantResult(const UAugmentDefinition& Definition, int32 Level, const FAugmentGrantHandles& Handles)
	{
		const int32 ExpectedEffects   = Definition.GrantedEffects.Num();
		const int32 ExpectedAbilities = Definition.GrantedAbilities.Num();

		if (Handles.EffectHandles.Num() == ExpectedEffects && Handles.AbilityHandles.Num() == ExpectedAbilities)
		{
			return;
		}

		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s %d레벨 부여가 일부 실패했습니다 (GE %d/%d, GA %d/%d). ")
			TEXT("하위 레벨은 이미 제거된 뒤라 그만큼 효과가 비어 있습니다. ")
			TEXT("[AugmentAssembler] 로그에서 실패한 항목을 확인하세요"),
			*Definition.GetName(), Level,
			Handles.EffectHandles.Num(), ExpectedEffects,
			Handles.AbilityHandles.Num(), ExpectedAbilities);
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

void UAugmentInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// 카탈로그는 비동기로 로드, 목록이 먼저 도착하면 정의를 못 찾음
	// 준비 되지 않은 상태면 OnRegistryReady 이벤트를 기다렸다가 처리
	if (UAugmentRegistrySubsystem* Registry = GetRegistry())
	{
		if (!Registry->IsReady())
		{
			Registry->OnRegistryReady.AddDynamic(this, &UAugmentInventoryComponent::HandleRegistryReady);

			// 해제할 때 쓸 참조를 지금 잡아둔다
			BoundRegistry = Registry;
		}
	}
}

void UAugmentInventoryComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (AbilitySystemComponent == InASC)
	{
		return;
	}

	// ASC 교체 시 기존 부여 효과 제거
	RemoveAllGrants();

	UninitializeFromAbilitySystem();

	AbilitySystemComponent = InASC;
}

void UAugmentInventoryComponent::UninitializeFromAbilitySystem()
{
	// 참조만 제거. 효과 제거는 InitializeWithAbilitySystem
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

bool UAugmentInventoryComponent::ServerAcquireAugment(UAugmentDefinition* Definition, int32 Level)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogAugmentInventory, Error, TEXT("[AugmentInventory] 권한이 없는 곳에서 증강을 획득하려 했습니다"));
		return false;
	}

	if (!Definition || !Definition->LineTag.IsValid())
	{
		UE_LOG(LogAugmentInventory, Error, TEXT("[AugmentInventory] 정의가 없거나 LineTag 가 비어 있습니다"));
		return false;
	}

	// 초기화가 빠지면 목록에는 들어가는데 효과는 적용 X
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s: ASC 가 없습니다. InitAbilityActorInfo 직후 InitializeWithAbilitySystem 호출이 빠졌습니다"),
			*GetNameSafe(GetOwner()));
		return false;
	}

	// 요청은 임의의 레벨로 올 수 있음
	if (!Definition->HasLevel(Level))
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s 에 %d레벨이 없습니다 (최고 %d레벨)"),
			*Definition->GetName(), Level, Definition->GetMaxLevel());
		return false;
	}

	const int32 CurrentLevel = GetLevelInLine(Definition->LineTag);

	// 레벨은 반드시 1씩 증가
	if (Level != CurrentLevel + 1)
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s 의 %d레벨을 요청했는데 현재 보유 레벨은 %d 입니다. 한 칸씩만 오를 수 있습니다"),
			*Definition->GetName(), Level, CurrentLevel);
		return false;
	}

	// 증강을 등록할 수 없으면 획득을 취소
	if (!RogueAugmentAssembler::CanApplyAugment(*Definition, Level))
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s %d레벨을 부여할 수 없어 획득을 취소합니다. 보유 상태는 그대로입니다"),
			*Definition->GetName(), Level);
		return false;
	}

	// 하위 레벨 증강 제거
	if (FAugmentGrantHandles* Existing = GrantHandlesByLine.Find(Definition->LineTag))
	{
		RogueAugmentAssembler::RemoveAugment(*AbilitySystemComponent, *Existing);
		GrantHandlesByLine.Remove(Definition->LineTag);
	}

	const int32 ExistingIndex = FindEntryIndexInLine(Definition->LineTag);
	if (ExistingIndex != INDEX_NONE)
	{
		Entries.RemoveAt(ExistingIndex);
	}

	// 새 증강 적용
	const FAugmentGrantHandles& Granted = GrantHandlesByLine.Add(Definition->LineTag,
		RogueAugmentAssembler::ApplyAugment(*AbilitySystemComponent, *Definition, Level));

	VerifyGrantResult(*Definition, Level, Granted);

	Entries.Add(FAugmentInventoryEntry::Make(*Definition, Level));

	RebuildConditionalRuleCache();

	// 서버에는 OnRep 이 오지 않으므로 직접 발신
	OnInventoryChanged.Broadcast();

	return true;
}

bool UAugmentInventoryComponent::ServerRollOffers(const FAugmentDraftContext& DraftContext, int32 CardCount)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogAugmentInventory, Error, TEXT("[AugmentInventory] 권한이 없는 곳에서 추첨을 돌렸습니다"));
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
	AcquireFromOffer(OfferIndex, TEXT("클라이언트 선택"));
}

bool UAugmentInventoryComponent::ServerAutoSelectOffer()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] 권한이 없는 곳에서 자동 선택이 호출됐습니다"));
		return false;
	}

	// 이미 고른 플레이어
	if (Offers.IsEmpty())
	{
		return false;
	}

	// 자동 선택은 지난 라운드 추첨 시드에서 갈라져 나온다. 같은 시드에서 뽑으면 같은 결과
	const uint32 AutoSelectSeed = HashCombine(static_cast<uint32>(LastDraftSeed), 0x9E3779B9u);
	FRandomStream Stream(static_cast<int32>(AutoSelectSeed));

	const int32 PickedIndex = Stream.RandRange(0, Offers.Num() - 1);

	UE_LOG(LogAugmentInventory, Log,
		TEXT("[AugmentInventory] %s: 자동 선택 %d번 (후보 %d장, 시드 %d)"),
		*GetNameSafe(Owner), PickedIndex, Offers.Num(), static_cast<int32>(AutoSelectSeed));

	return AcquireFromOffer(PickedIndex, TEXT("자동 선택"));
}

bool UAugmentInventoryComponent::AcquireFromOffer(int32 OfferIndex, const TCHAR* Reason)
{
	const AActor* Owner = GetOwner();

	// 후보가 비어 있는 경우는 이미 고른 경우
	if (Offers.IsEmpty())
	{
		UE_LOG(LogAugmentInventory, Warning,
			TEXT("[AugmentInventory] %s: 후보가 없는데 %s 이 들어왔습니다. 이미 선택했거나 페이즈가 끝났습니다"),
			*GetNameSafe(Owner), Reason);
		return false;
	}

	if (!Offers.IsValidIndex(OfferIndex))
	{
		UE_LOG(LogAugmentInventory, Warning,
			TEXT("[AugmentInventory] %s: %s 인덱스 %d 가 후보 %d개 범위 밖입니다"),
			*GetNameSafe(Owner), Reason, OfferIndex, Offers.Num());
		return false;
	}

	UAugmentDefinition* Definition = ResolveDefinition(Offers[OfferIndex].DefinitionId);
	if (!Definition)
	{
		UE_LOG(LogAugmentInventory, Error,
			TEXT("[AugmentInventory] %s: 후보 %d 의 정의 %s 를 레지스트리에서 찾지 못했습니다"),
			*GetNameSafe(Owner), OfferIndex, *Offers[OfferIndex].DefinitionId.ToString());
		return false;
	}

	// 획득 시도. 실패하면 후보를 그대로 두고 다시 선택 가능
	if (!ServerAcquireAugment(Definition, Offers[OfferIndex].Level))
	{
		return false;
	}

	// 후보 목록을 비워 선택 완료 상태로 변경
	Offers.Reset();
	OnOffersChanged.Broadcast();

	return true;
}

void UAugmentInventoryComponent::ServerRemoveAllAugments()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

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

TArray<UAugmentDefinition*> UAugmentInventoryComponent::GetOwnedDefinitions() const
{
	TArray<UAugmentDefinition*> Result;
	Result.Reserve(Entries.Num());

	for (const FAugmentInventoryEntry& Entry : Entries)
	{
		if (UAugmentDefinition* Definition = ResolveDefinition(Entry.DefinitionId))
		{
			Result.Add(Definition);
		}
	}

	return Result;
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
	// 레지스트리 이벤트 해제
	if (UAugmentRegistrySubsystem* Registry = BoundRegistry.Get())
	{
		Registry->OnRegistryReady.RemoveDynamic(this, &UAugmentInventoryComponent::HandleRegistryReady);
		BoundRegistry.Reset();
	}

	UninitializeFromAbilitySystem();

	Super::OnUnregister();
}

void UAugmentInventoryComponent::OnRep_Entries()
{
	RebuildConditionalRuleCache();

	OnInventoryChanged.Broadcast();
}

void UAugmentInventoryComponent::OnRep_Offers()
{
	OnOffersChanged.Broadcast();
}

void UAugmentInventoryComponent::HandleRegistryReady()
{
	// 목록 재생성
	RebuildConditionalRuleCache();

	OnInventoryChanged.Broadcast();
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

int32 UAugmentInventoryComponent::FindEntryIndexInLine(const FGameplayTag& LineTag) const
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (Entries[Index].LineTag.MatchesTagExact(LineTag))
		{
			return Index;
		}
	}

	return INDEX_NONE;
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
