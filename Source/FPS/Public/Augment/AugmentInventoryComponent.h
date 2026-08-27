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
struct FAugmentDraftEligibility;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAugmentInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAugmentOffersChanged);

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

	// 매치 초기화. 서버 전용
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rogue|Augment")
	void RemoveAllAugments();

	// 드래프트 매니저가 추첨한 결과를 후보로 저장, 서버 전용
	// SlotCards 는 슬롯별 카드 체인, 각 체인의 첫 장이 이번에 제시할 후보
	bool SetOffers(const TArray<TArray<FAugmentInventoryEntry>>& SlotCards, int32 DraftSeed);

	// 후보 판정에 필요한 인벤토리 상태를 채워 반환, 서버 전용
	void FillDraftEligibility(FAugmentDraftEligibility& OutState) const;

	// 제시된 후보를 매치 등장 이력에 남긴다. 서버 전용
	void MarkOffersSeen(const TArray<FAugmentInventoryEntry>& InOffers);

	// 카드 선택. 소유 클라이언트가 호출. 액티브 슬롯은 서버가 배정
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rogue|Augment")
	void ServerSelectOffer(int32 OfferIndex);

	// 슬롯 하나만 다시 뽑음. 소유 클라이언트가 호출
	// 비용 없음. 추첨은 라운드 준비 때 완료된 상태
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rogue|Augment")
	void ServerRerollOffer(int32 OfferIndex);

	// 이 슬롯에 남은 리롤 횟수. 후보 범위 밖이면 0
	// 0보다 크면 UI의 버튼 활성화
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	int32 GetSlotRerollsRemaining(int32 OfferIndex) const;

	// 후보 생성과 실제 선택 가능 시점을 분리한다. DraftManager만 호출하는 서버 API.
	bool SetOfferSelectionEnabled(bool bEnabled);

	// 미선택자 자동 선택. 서버 전용
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rogue|Augment")
	bool AutoSelectOffer();

	// 라운드 준비가 취소됐을 때 보유 증강은 유지하고 미선택 후보만 폐기. 서버 전용
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rogue|Augment")
	void ClearOffers();


	// 증강 보상 큐에 증강 정보 등록 서버 전용
	void PushPendingReward(const FAugmentPendingReward& Reward);

	// 대기열 첫 항목. 비어 있으면 nullptr
	const FAugmentPendingReward* PeekPendingReward() const;

	// 첫 항목을 버린다. 서버 전용
	void PopPendingReward();

	// 선택 마감 시각(서버 월드 시간). 0 이면 제한 없음
	// 클라이언트는 GameState 의 GetServerWorldTimeSeconds 와 비교해 남은 시간을 구한다
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	float GetSelectionDeadline() const { return SelectionDeadline; }

	// 제한 시간 안에 고르지 않으면 자동 선택. 서버 전용
	void StartSelectionTimeout(float Seconds);
	void ClearSelectionTimeout();

	// 이번 라운드에 제시된 후보
	// 비어있는 경우 = 선택 완료
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	const TArray<FAugmentInventoryEntry>& GetOffers() const { return Offers; }

	// 현재 보유한 증강 목록, 서버에서만 변경하며 클라이언트에게 복제
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	const TArray<FAugmentInventoryEntry>& GetEntries() const { return Entries; }

	// ExecCalc(서버전용)이 히트마다 읽음 정의를 매번 훑지 않도록 캐싱
	const TArray<FAugmentConditionalDamageRule>& GetConditionalDamageRules() const { return CachedConditionalRules; }

	// ExecCalc 은 ASC 만 들고 있다. 거기서 인벤토리로 건너오는 유일한 경로
	static UAugmentInventoryComponent* FindInventory(const UAbilitySystemComponent* ASC);

protected:
	virtual void OnUnregister() override;

private:
	UFUNCTION()
	void OnRep_Entries();

	UFUNCTION()
	void OnRep_Offers();

	void HandleSelectionTimeout();

	UAugmentRegistrySubsystem* GetRegistry() const;
	UAugmentDefinition* ResolveDefinition(const FPrimaryAssetId& DefinitionId) const;

	// 서버 전용 함수의 공통 가드
	bool HasServerAuthority(const TCHAR* Context) const;

	int32 FindEntryIndexInLine(const FGameplayTag& LineTag) const;
	void RemoveLineAugment(const FGameplayTag& LineTag);
	bool ApplyLineAugment(UAugmentDefinition& Definition);
	void RebuildConditionalRuleCache();

	// 후보 하나를 골라 즉시 부여한다. 서버 전용
	bool SelectAndApplyOffer(int32 OfferIndex, const TCHAR* Reason);

	// 이번 라운드 드래프트 상태 초기화
	void ClearRoundDraftState();

	// 참조만 끊는다. 효과 제거는 교체 경로가
	void UninitializeFromAbilitySystem();

	// 부여한 GE/GA 를 현재 ASC 에서 전부 제거
	void RemoveAllGrants();

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// 보유 목록. 서버에서만 변경하고 소유자에게만 복제
	// 수명은 매치 내내. 비어 있다 = 증강 없음
	UPROPERTY(ReplicatedUsing = OnRep_Entries)
	TArray<FAugmentInventoryEntry> Entries;

	// 이번 라운드 후보. 서버에서만 변경하고 소유자에게만 복제
	UPROPERTY(ReplicatedUsing = OnRep_Offers)
	TArray<FAugmentInventoryEntry> Offers;

	// 슬롯별 잔여 리롤 횟수
	UPROPERTY(Replicated)
	TArray<int32> SlotRerollsRemaining;

	// 슬롯별 리롤 예비, 서버 전용
	TArray<TArray<FAugmentInventoryEntry>> SlotReserves;

	// 서버 전용. 계열별 해제 핸들
	UPROPERTY()
	TMap<FGameplayTag, FAugmentGrantHandles> GrantHandlesByLine;

	TArray<FAugmentConditionalDamageRule> CachedConditionalRules;

	// 이 매치에서 한 번이라도 후보로 제시된 증강, 서버 전용
	// 한번 등장한 증강은 재등장하지 않음
	TSet<FPrimaryAssetId> SeenDefinitions;

	// 서버 전용. 매니저가 이번 라운드 추첨에 쓴 시드.
	// 자동 선택이 이걸 재사용해야 "이 라운드에 왜 이 카드가 자동 선택됐나" 를 재현 가능
	int32 LastDraftSeed = 0;

	// Offers가 복제됐더라도 DraftManager가 선택 페이즈를 열기 전에는 RPC를 거부한다.
	bool bOfferSelectionEnabled = false;

	// 적립된 증강 보상 후보. 서버 전용
	TArray<FAugmentPendingReward> PendingRewards;

	UPROPERTY(Replicated)
	float SelectionDeadline = 0.f;

	FTimerHandle SelectionTimerHandle;

public:
	// 보유 목록이 바뀔 때. 서버는 변경 시점에, 소유 클라이언트는 복제 도착 시점에 발신
	UPROPERTY(BlueprintAssignable, Category = "Rogue|Augment")
	FOnAugmentInventoryChanged OnInventoryChanged;

	// 후보가 바뀔 때. 서버는 변경 시점에, 소유 클라이언트는 복제 도착 시점에 발신
	UPROPERTY(BlueprintAssignable, Category = "Rogue|Augment")
	FOnAugmentOffersChanged OnOffersChanged;
};
