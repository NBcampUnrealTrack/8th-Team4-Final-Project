// ShooterAugmentSelectBanner.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "ShooterAugmentSelectBanner.generated.h"

class UAugmentInventoryComponent;
class UAugmentRegistrySubsystem;

UENUM(BlueprintType)
enum class EAugmentBannerKind : uint8
{
	// 후보 없음 배너 감춤
	None,
	// 액티브 증강만
	Active,
	// 패시브 증강만
	Passive,
	// 섞여 있을때
	Mixed
};

/**
 * 증강 선택 안내 문구와 남은 선택 시간을 나타낸다 
 * PendingCard 상태일때는 HUD Hint를 띄우고 Card는 닫는다
 */
UCLASS(Abstract, Blueprintable, meta = (DisableNativeTick))
class FPS_API UShooterAugmentSelectBanner : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// 카드 선택 화면과 보류 HUD 힌트가 각각 가진 배너 중
	// 현재 화면에 보이는 쪽만 남은 시간 표시를 갱신하도록 한다.
	UFUNCTION(BlueprintCallable, Category = "Augment|Banner")
	void SetPresentationActive(bool bActive);
	
	UFUNCTION(BlueprintPure, Category = "Augment|Banner")
	EAugmentBannerKind GetBannerKind() const;
	UFUNCTION(Blueprintable, Category = "Augment|Banner")
	FText GetSlotDisplayName() const;
	UFUNCTION(BlueprintPure, Category = "Augment|Banner")
	int32 GetDisplaySeconds() const;
	// 제한 시간이 걸려져 있는지
	UFUNCTION(BlueprintPure, Category = "Augment|Banner")
	bool HasSelectionDeadline() const;
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Augment|Banner", 
		meta = (DisplayName = "배너 문구 갱신"))
	void BP_OnBannerKindChanged(EAugmentBannerKind Kind, const FText& InSlotDisplayName);
	UFUNCTION(BlueprintImplementableEvent, Category = "Augment|Banner",
		meta = (DisplayName = "남은 시간 갱신"))
	void BP_OnDisplaySecondsChanged(int32 Seconds, bool bInHasDeadline);
	UFUNCTION(BlueprintImplementableEvent, Category = "Augment|Banner",
	meta = (DisplayName = "남은 시간 머터리얼 갱신"))
	void BP_OnCountdownProgressChanged(float InProgress);
	
	UPROPERTY(EditDefaultsOnly, Category = "Augment|Banner",
		meta = (Categories = "InputTag.Abililty", ForceInlineRow))
	TMap<FGameplayTag, FText> SlotDisplayNames;
	
	UPROPERTY(EditDefaultsOnly, Category = "Augment|Banner", meta = (ClampMin = "0.05"))
	float CountdownInterval = 0.05f;
	
private:
	// OnOffersChanged를 즉시 처리하지 않고 다음 틱으로 합친다.
	void ScheduleBannerRefresh();
	void TryBindInventory();
	// 풀에서 재사용된 인스턴스의 낡은 값을 지움
	void ResetTransientState();
	void RefreshBannerKind();
	
	void RefreshCountdown();
	void StartCountdown();
	void StopCountdown();
	
	UFUNCTION()
	void HandleOffersChanged();
	UFUNCTION()
	void HandleRegistryReady();
	UFUNCTION()
	void HandleScheduledBannerRefresh();
	
	UAugmentInventoryComponent* GetInventory() const;
	UAugmentRegistrySubsystem* GetRegistry() const;
	
	TWeakObjectPtr<UAugmentInventoryComponent> BoundInv;
	FTimerHandle CountdownTimerHandle;
	FTimerHandle InventoryBindRetryTimerHandle;
	FTimerHandle BannerRefreshTimerHandle;
	
	// 전부 Transient로 ResetTransientState()에서 초기화
	EAugmentBannerKind BannerKind = EAugmentBannerKind::None;
	FText SlotDisplayName;
	int32 DisplaySeconds = 0;
	bool bHasDeadline = false;
	bool bPresentationActive = false;
	bool bRegistryReadyBound = false;
	bool bBannerRefreshPending = false;
	
	static constexpr float InventoryBindRetryInterval = 0.25f;
	static constexpr float TotalDuration = 20.f;
};
