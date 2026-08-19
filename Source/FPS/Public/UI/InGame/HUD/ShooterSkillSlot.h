// ShooterSkillSlotWidget.h
#pragma once

#include "CoreMinimal.h"
#include "ShooterAttributeWidget.h"
#include "GameplayTagContainer.h"
#include "ShooterSkillSlot.generated.h"

class UShooterGauge;
class UImage;
class UHorizontalBox;
class UCommonTextBlock;
class UWidgetAnimation;
class UShooterChargeBar;
class AShooterPlayerState;
class UAugmentInventoryComponent;
class URetainerBox;
class UMaterialInstanceDynamic;

/**
 * 스킬 슬롯(Slot1/Slot2) 하나의 쿨다운+충전 표시 위젯
 */
UCLASS()
class FPS_API UShooterSkillSlot : public UShooterAttributeWidget
{
	GENERATED_BODY()
	
public:
	// 쿨타임 중 매 업데이트하는 실데이터 폴링 진입점
	UFUNCTION(BlueprintCallable, Category = "Skill Slot")
	void SetCooldownState(float RemainingSeconds, float TotalCooldown);
	
	// Skill Charge 개수 갱신하며 매 충전 회복/소모 시 호출
	UFUNCTION(BlueprintCallable, Category = "Skill Sllot")
	void SetCharges(int32 AvailableCharges, int32 InMaxCharges);
	
	// 쿨타임이 끝난 순간 1회 작동. 게이지/텍스트 숨김 처리
	UFUNCTION(BlueprintCallable, Category = "Skill Slot")
	void SetReady();
	
	// 개발 중 검증 전용. 캐스트 1회에 대응. 충전 하나를 소모하고 쿨다운을 시작한다.
	UFUNCTION(BlueprintCallable, Category = "Skill Slot|Debug")
	void Debug_ConsumeCharge();
	
	// 이 슬롯이 어느 InputTag(Slot1/Slot2)에 대응하는지 인스턴스별로 지정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot")
	FGameplayTag SlotInputTag;
	
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
	virtual void OnAbilitySystemReady(URogueAbilitySystemComponent* ASC) override;
	virtual void OnAbilitySystemLost(URogueAbilitySystemComponent* ASC) override;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterGauge> Gauge_Cooldown;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Cooldown;
	// MaxCharges만큼 채움
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> HBox_ChargeBars;
	// ChargeBars 위젯 클래스 단순 UImage
	UPROPERTY(EditDefaultsOnly, Category = "Skill Slot")
	TSubclassOf<UShooterChargeBar> ChargeBarWidgetClass;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_ReadyGlow;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_ReadyGlow;
	
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_CooldownReject;
	
	// 글리치를 위한 RetainerBox 
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<URetainerBox> RetainerBox_Glitch;
	// 런타임 파라미터 제어에 사용
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CachedGlitchMID;
	
	// Skill 밑 Key 표시용
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_KeyLabel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot")
	FText KeyLabel;
	
	// true면 Debug_ConsumeCharge로 구동. 증강없이 비주얼 검증 위함
	// false면 실제 ASC 폴링. 실연동 후에도 디버그 경로는 남겨둬야
	// 비주얼만 다시 손볼 때 증강을 뽑지 않고 바로 켜서 테스트 가능
	UPROPERTY(EditDefaultsOnly, Category = "Skill Slot")
	bool bUseDebugDriver = true;
	
	// 디버그 전용 설정
	UPROPERTY(EditDefaultsOnly, Category = "Skill Slot|Debug")
	float Debug_CooldownSeconds = 5.f;
	// 다충전 순차 회복은 이 값을 2~3으로 올려서 검증
	UPROPERTY(EditDefaultsOnly, Category = "Skill Slot|Debug")
	int32 Debug_MaxCharges = 2;
	
private:
	// Debug 데이터와 GAS 실제 데이터를 동일한 형태로 전달하기 위한 중간 상태 데이터
	// 두 데이터 소스는 서로 다른 방식으로 값을 계싼하지만,
	// RefreshCooldownDisplay()는 동일한 표시 로직을 수행해야 하므로
	// 공통 입력 형태를 보장하기 위해 사용
	struct FSkillSlotState
	{
		int32 SpentCharges = 0;
		float NextRecoveryRemaining = 0.f;
		
		// bChargeInfoAvailable = true일때만 유효
		bool bChargeInfoAvailable = false;
		int32 MaxCharges = 1;
		float CooldownDuration = 0.f;
	};
	
	bool BuildStateFromDebug(FSkillSlotState& Out) const;
	bool BuildStateFromAbility(FSkillSlotState& Out) const;
	
	// 갱신 창구. 트리거(Event / Debug OneShotTimer)가 있을 때만 불림
	void RefreshCooldownDisplay();
	
	void RebuildChargeBars(int32 InMaxCharges);
	
	// InventoryRetryAttemptsLeft가 남아있는 동안 TryBindCooldownTag를 짧게 재시도
	// OnAbilitySystemReady/HandleInventoryChanged 양쪽에서 재시도 카운트 리셋하고 호출하는 진입점
	void AttemptBindCooldownTag();
	
	// CachedCooldownTag를 아직 못 찾았으면 SlotInputTag로 탐색 시도
	void TryBindCooldownTag(URogueAbilitySystemComponent* ASC);
	
	// DELEGATE 맞추기용
	UFUNCTION()
	void HandleInventoryChanged();
	void HandleCooldownTagChanged(FGameplayTag Tag, int32 NewCount);
	
	UFUNCTION()
	void HandleGlowReadyAnimFinished();
	
	// Cooldown시 실패 피드백위함
	UFUNCTION()
	void HandleAbilityFailed(
		const UGameplayAbility* Ability, 
		const FGameplayTagContainer& FailureReason);
	
	UMaterialInstanceDynamic* GetGlitchMID();
	
	// 매 갱신마다 재생성하지 않고 이 배열의 SetFilled만 호출한다
	UPROPERTY(Transient)
	TArray<TObjectPtr<UShooterChargeBar>> ChargeBars;
	
	// 마지막으로 표시한 사용 가능 충전 수 전환을 감지해 Glow 재생 위함
	int32 LastAvailableCharges = -1;
	
	int32 LastDisplayedTenths = INDEX_NONE;
	
	// 각 항목이 충전 하나가 회복되는 월드 시각
	TArray<float> DebugPendingRecoveryTimes;
	FTimerHandle DebugNextRecoveryTimerHandle;
	
	// 실데이터 이벤트 구독 상태
	FGameplayTag CachedCooldownTag;
	FDelegateHandle CooldownTagEventHandle;
	FDelegateHandle AbilityFailedEventHandle;
	TWeakObjectPtr<UAugmentInventoryComponent> BoundInventory;
	
	// OnInventoryChanged와 ASC::ActivatableAbilities 복제 순서가 복장되지 않아서 필요한
	// bounded retry 이 횟수만큼 짧게 재확인하고 멈춤
	static constexpr int32 MaxInventoryRetryAttempts = 5;
	int32 InventoryRetryAttemptsLeft = 0;
	FTimerHandle InventoryRetryTimerHandle;
	
	// 로컬 시간 진행용 NativeTick은 이 두값만 읽고 ASC/디버그 배열은 안건드림
	float RecoveryEndWorldTime = 0.f; // 0이면 READY(진행 중 회복 없음)
	float DisplayCooldownDuration = 0.f;
};
