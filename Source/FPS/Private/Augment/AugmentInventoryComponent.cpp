// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/AugmentInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogAugmentInventory, Log, All);

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
		}
	}
}

void UAugmentInventoryComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (AbilitySystemComponent == InASC)
	{
		return;
	}

	UninitializeFromAbilitySystem();

	AbilitySystemComponent = InASC;
}

void UAugmentInventoryComponent::UninitializeFromAbilitySystem()
{
	GrantHandlesByLine.Reset();
	AbilitySystemComponent = nullptr;
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
	GrantHandlesByLine.Add(Definition->LineTag,
		RogueAugmentAssembler::ApplyAugment(*AbilitySystemComponent, *Definition, Level));

	FAugmentInventoryEntry Entry;
	Entry.DefinitionId = Definition->GetPrimaryAssetId();
	Entry.Level        = Level;
	Entries.Add(Entry);

	RebuildConditionalRuleCache();

	// 서버에는 OnRep 이 오지 않으므로 직접 발신
	OnInventoryChanged.Broadcast();

	return true;
}

void UAugmentInventoryComponent::ServerRemoveAllAugments()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (AbilitySystemComponent)
	{
		for (TPair<FGameplayTag, FAugmentGrantHandles>& Pair : GrantHandlesByLine)
		{
			RogueAugmentAssembler::RemoveAugment(*AbilitySystemComponent, Pair.Value);
		}
	}

	GrantHandlesByLine.Reset();
	Entries.Reset();
	CachedConditionalRules.Reset();

	OnInventoryChanged.Broadcast();
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
	UninitializeFromAbilitySystem();

	Super::OnUnregister();
}

void UAugmentInventoryComponent::OnRep_Entries()
{
	RebuildConditionalRuleCache();

	OnInventoryChanged.Broadcast();
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
		const UAugmentDefinition* Definition = ResolveDefinition(Entries[Index].DefinitionId);
		if (Definition && Definition->LineTag.MatchesTagExact(LineTag))
		{
			return Index;
		}
	}

	return INDEX_NONE;
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
