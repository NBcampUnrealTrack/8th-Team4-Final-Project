// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Augment/AugmentAssembler.h"
#include "Augment/AugmentTypes.h"
#include "AugmentInventoryComponent.generated.h"

class UAbilitySystemComponent;
class UAugmentDefinition;
class UAugmentRegistrySubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAugmentInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAugmentOffersChanged);

// 서버 전용 드래프트 선택 커밋 상태.
enum class EAugmentOfferCommitState : uint8
{
	Empty,
	Selected,
	Applied,
	RecoveryPending
};

struct FAugmentLineSnapshot
{
	FGameplayTag LineTag;
	TOptional<FAugmentInventoryEntry> Entry;
	int32 EntryIndex = INDEX_NONE;

	void Reset()
	{
		LineTag = FGameplayTag();
		Entry.Reset();
		EntryIndex = INDEX_NONE;
	}
};

UCLASS(ClassGroup = (Rogue), meta = (BlueprintSpawnableComponent))
class FPS_API UAugmentInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAugmentInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ASC 는 PlayerState 에 있어 이 컴포넌트의 BeginPlay 시점에 없을 수 있음
	// 소유자가 InitAbilityActorInfo 직후에 직접 호출
	UFUNCTION(BlueprintCallable, Category = "Rogue|Augment")
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);

	// 증강 획득. 서버 전용
	// 같은 계열을 이미 보유하고 있으면 하위 레벨을 걷어내고 교체
	UFUNCTION(BlueprintCallable, Category = "Rogue|Augment")
	bool ServerAcquireAugment(UAugmentDefinition* Definition, int32 Level = 1);

	// 매치 초기화. 서버 전용
	UFUNCTION(BlueprintCallable, Category = "Rogue|Augment")
	void ServerRemoveAllAugments();

	// 이번 라운드 후보를 뽑아 Offers 에 채움, 서버 전용
	// 카드마다 등급을 굴리고, 그 등급 버킷에서 균등 추출
	UFUNCTION(BlueprintCallable, Category = "Rogue|Augment")
	bool ServerRollOffers(const FAugmentDraftContext& DraftContext, int32 CardCount = 3);

	// 카드 선택. 이 클래스의 유일한 실제 RPC, 소유 클라이언트가 호출
	// 정의가 아니라 인덱스를 받음 - 후보 외의 증강은 선택할 수 없음
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rogue|Augment")
	void ServerSelectOffer(int32 OfferIndex);

	// 후보 생성과 실제 선택 가능 시점을 분리한다. DraftManager만 호출하는 서버 API.
	bool ServerSetOfferSelectionEnabled(bool bEnabled);

	// 미선택자 자동 선택. 서버 전용
	UFUNCTION(BlueprintCallable, Category = "Rogue|Augment")
	bool ServerAutoSelectOffer();

	// 선택은 먼저 스테이징하고, 모든 참가자가 확정 가능한 경우 DraftManager가 일괄 커밋한다.
	bool HasStagedOfferSelection() const { return StagedOfferState != EAugmentOfferCommitState::Empty; }
	bool CanCommitStagedOffer() const;
	bool CommitStagedOffer();
	void FinalizeStagedOfferCommit();
	bool RollbackStagedOfferCommit();

	// 라운드 준비가 취소됐을 때 보유 증강은 유지하고 미선택 후보만 폐기. 서버 전용
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rogue|Augment")
	void ServerClearOffers();

	// 이번 라운드에 제시된 후보
	// 비어있는 경우 = 선택 완료
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	TArray<FAugmentInventoryEntry> GetOffers() const { return Offers; }

	// 이 라운드의 등급 가중치
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	static float GetTierDraftWeight(EAugmentTier Tier, int32 RoundNumber);

	// 현재 보유한 증강 목록, 서버에서만 변경하며 클라이언트에게 복제
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	TArray<FAugmentInventoryEntry> GetEntries() const { return Entries; }

	// 정의까지 해석한 목록
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	TArray<UAugmentDefinition*> GetOwnedDefinitions() const;

	// 이 계열의 보유 레벨. 미보유면 0.
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	int32 GetLevelInLine(FGameplayTag LineTag) const;

	// ExecCalc 이 히트마다 읽는다. 정의를 매번 훑지 않도록 캐싱
	const TArray<FAugmentConditionalDamageRule>& GetConditionalDamageRules() const { return CachedConditionalRules; }

	// ExecCalc 은 ASC 만 들고 있다. 거기서 인벤토리로 건너오는 유일한 경로
	static UAugmentInventoryComponent* FindInventory(const UAbilitySystemComponent* ASC);

protected:
	virtual void BeginPlay() override;
	virtual void OnUnregister() override;

private:
	UFUNCTION()
	void OnRep_Entries();

	UFUNCTION()
	void OnRep_Offers();

	UFUNCTION()
	void HandleRegistryReady();

	UAugmentRegistrySubsystem* GetRegistry() const;
	UAugmentDefinition* ResolveDefinition(const FPrimaryAssetId& DefinitionId) const;

	int32 FindEntryIndexInLine(const FGameplayTag& LineTag) const;
	FAugmentLineSnapshot CaptureLineSnapshot(const FGameplayTag& LineTag) const;
	void RemoveLineAugment(const FGameplayTag& LineTag);
	bool ApplyLineAugment(UAugmentDefinition& Definition, int32 Level, int32 EntryIndex);
	bool RestoreLineSnapshot(const FAugmentLineSnapshot& Snapshot);
	void RebuildConditionalRuleCache();

	// 등급별 후보. 인덱스가 EAugmentTier 값이고 크기는 항상 MAX
	void GatherCandidatesByTier(TArray<TArray<FAugmentInventoryEntry>>& OutBuckets) const;

	// 선택한 후보를 이번 드래프트의 확정 대기 상태로 옮긴다. 서버 전용
	bool StageOfferSelection(int32 OfferIndex, const TCHAR* Reason);
	void ClearStagedOffer();

	// 참조만 끊는다. 효과 제거는 교체 경로가
	void UninitializeFromAbilitySystem();

	// 부여한 GE/GA 를 현재 ASC 에서 전부 제거
	void RemoveAllGrants();

	// 플레이어별로 다른 시드 생성
	int32 MakeDraftSeed(const FAugmentDraftContext& DraftContext) const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// 보유 목록. 서버에서만 변경하고 소유자에게만 복제
	// 수명은 매치 내내. 비어 있다 = 증강 없음
	UPROPERTY(ReplicatedUsing = OnRep_Entries)
	TArray<FAugmentInventoryEntry> Entries;

	// 이번 라운드 후보. 서버에서만 변경하고 소유자에게만 복제
	UPROPERTY(ReplicatedUsing = OnRep_Offers)
	TArray<FAugmentInventoryEntry> Offers;

	// 서버 전용. 계열별 해제 핸들
	UPROPERTY()
	TMap<FGameplayTag, FAugmentGrantHandles> GrantHandlesByLine;

	TArray<FAugmentConditionalDamageRule> CachedConditionalRules;

	// 구독해 둔 레지스트리
	TWeakObjectPtr<UAugmentRegistrySubsystem> BoundRegistry;

	// 서버 전용. 마지막 추첨에 쓴 시드.
	// 자동 선택이 이걸 재사용해야 "이 라운드에 왜 이 카드가 자동 선택됐나" 를 재현 가능
	int32 LastDraftSeed = 0;

	// Offers가 복제됐더라도 DraftManager가 선택 페이즈를 열기 전에는 RPC를 거부한다.
	bool bOfferSelectionEnabled = false;

	// 서버 전용. 선택은 모든 참가자의 확정 가능 여부를 확인할 때까지 실제 보유 목록에 적용하지 않는다.
	FAugmentInventoryEntry StagedOffer;
	FAugmentLineSnapshot StagedPreviousLine;
	EAugmentOfferCommitState StagedOfferState = EAugmentOfferCommitState::Empty;

public:
	// 보유 목록이 바뀔 때. 서버는 변경 시점에, 소유 클라이언트는 복제 도착 시점에 발신
	UPROPERTY(BlueprintAssignable, Category = "Rogue|Augment")
	FOnAugmentInventoryChanged OnInventoryChanged;

	// 후보가 바뀔 때. 서버는 변경 시점에, 소유 클라이언트는 복제 도착 시점에 발신
	UPROPERTY(BlueprintAssignable, Category = "Rogue|Augment")
	FOnAugmentOffersChanged OnOffersChanged;
};
