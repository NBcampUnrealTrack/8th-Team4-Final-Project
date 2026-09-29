// ShooterSkillSlotWidget.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "ShooterAttributeWidget.h"
#include "Engine/StreamableManager.h"
#include "GameplayTagContainer.h"
#include "ShooterSkillSlot.generated.h"

class UGameplayAbility;
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
class UTextBlock;
class UTexture2D;
class UMaterialInterface;

/**
 * 스킬 슬롯(Slot1/Slot2/Slot3) 하나의 쿨다운 + 충전 + 아이콘 표시 위젯
 *
 * 구현이 세 파일로 나뉜다.
 *   ShooterSkillSlot.cpp          ASC 배관
 *   ShooterSkillSlot_Display.cpp  취득 → 계산 → 적용 (쿨다운, 충전)
 *   ShooterSkillSlot_Icon.cpp     아이콘 수명 (조회, 비동기 로드)
 */
UCLASS()
class FPS_API UShooterSkillSlot : public UShooterAttributeWidget
{
	GENERATED_BODY()

public:
	// ── 인스턴스 설정 ──

	// 이 슬롯이 어느 InputTag(Slot1/Slot2)에 대응하는지 인스턴스별로 지정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot")
	FGameplayTag SlotInputTag;

	// ── 표시 진입점 ──

	// 쿨타임 중 매 업데이트하는 실데이터 폴링 진입점
	UFUNCTION(BlueprintCallable, Category = "Skill Slot")
	void SetCooldownState(float RemainingSeconds, float TotalCooldown);

	// Skill Charge 개수 갱신하며 매 충전 회복/소모 시 호출
	UFUNCTION(BlueprintCallable, Category = "Skill Slot")
	void SetCharges(int32 AvailableCharges, int32 InMaxCharges);

	// 쿨타임이 끝난 순간 1회 작동. 게이지/텍스트 숨김 처리
	UFUNCTION(BlueprintCallable, Category = "Skill Slot")
	void SetReady();

	// 개발 중 검증 전용. 캐스트 1회에 대응. 충전 하나를 소모하고 쿨다운을 시작한다.
	UFUNCTION(BlueprintCallable, Category = "Skill Slot|Debug")
	void Debug_ConsumeCharge();

	// ── 아이콘 주입 창구 ──
	// 아이콘을 찾아오는 쪽은 이 둘만 안다. Image_Icon 도 머티리얼도 모른다

	// 런타임 아이콘 변경
	void SetIconTexture(UTexture2D* InTexture);

	// DefaultIconTexture로 되돌린다
	void ResetIconTexture();

	// ══════════════════════════════════════════════════════════════
	//  위젯 수명 · 바인딩 · 에디터 설정
	// ══════════════════════════════════════════════════════════════
protected:
	// ── 위젯 수명 ──

	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual void OnAbilitySystemReady(URogueAbilitySystemComponent* ASC) override;
	virtual void OnAbilitySystemLost(URogueAbilitySystemComponent* ASC) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UShooterGauge> Gauge_Cooldown;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Cooldown;
	// Skill 밑 Key 표시용
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_KeyLabel;
	// MaxCharges만큼 채움
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> HBox_ChargeBars;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Icon;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Frame;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_ReadyGlow;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_KeyFill;
	// 글리치를 위한 RetainerBox
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<URetainerBox> RetainerBox_Glitch;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_ReadyGlow;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Ready;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_CooldownReject;

	// ── 에디터 설정 ──

	// ChargeBars 위젯 클래스 단순 UImage
	UPROPERTY(EditDefaultsOnly, Category = "Skill Slot")
	TSubclassOf<UShooterChargeBar> ChargeBarWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot")
	FText KeyLabel;

	// 증강이 없거나 Icon이 비어있을때 폴백
	UPROPERTY(EditAnywhere, Category = "SKill Slot")
	TObjectPtr<UTexture2D> DefaultIconTexture;

	// true면 Debug_ConsumeCharge로 구동. 증강없이 비주얼 검증 위함
	UPROPERTY(EditDefaultsOnly, Category = "Skill Slot")
	bool bUseDebugDriver = true;

	// 디버그 전용 설정
	UPROPERTY(EditDefaultsOnly, Category = "Skill Slot|Debug")
	float Debug_CooldownSeconds = 5.f;
	// 다충전 순차 회복은 이 값을 2~3으로 올려서 검증
	UPROPERTY(EditDefaultsOnly, Category = "Skill Slot|Debug")
	int32 Debug_MaxCharges = 2;

	// ══════════════════════════════════════════════════════════════
	//  아이콘 수명            → ShooterSkillSlot_Icon.cpp
	// ══════════════════════════════════════════════════════════════

	void RefreshSlotIcon();
	void BeginIconLoad(
		const FPrimaryAssetId& RequestedId,
		const FSoftObjectPath& IconPath);

	// 로드를 취소하고 Pending을 비움
	void ClearIconLoad();
	// 화면과 캐시를 플레이스홀더로 바꿈
	void ResetIconDisplay();
	// 로드를 취소하고 처리 완료로 캐시
	void ClearSlotIcon(const FPrimaryAssetId& ProcessedId);
	// 엔트리가 있는 슬롯의 Visibility를 되살림
	void ApplyOccupiedState();
	// 슬롯이 비었을때 전면 초기화
	void ApplyEmptySlotState();
	void ReleaseIconResources();

	// 아이콘 조회 대기 상태 체크
	bool IsIconResolutionSettled() const;

	// 현재 Image_Icon에 있는 Icon
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CachedSlotIcon;

	// 처리 완료된 정의 ID - 동일 정의의 중복 로드 방지
	FPrimaryAssetId CachedIconDefinitionId;
	// 재 로드 요청 중인 정의 ID - 중복 요청 및 stale 콜백 판별용
	FPrimaryAssetId PendingIconDefinitionId;

	TSharedPtr<FStreamableHandle> IconLoadHandle;

	FGameplayAbilitySpecHandle CachedAbilitySpecHandle;

	// ══════════════════════════════════════════════════════════════
	//  표시 상태 타입
	// ══════════════════════════════════════════════════════════════
private:
	// Debug 데이터와 GAS 실제 데이터를 동일한 형태로 전달하기 위한 중간 상태 데이터
	struct FSkillSlotState
	{
		int32 SpentCharges = 0;
		float NextRecoveryRemaining = 0.f;
		// 쿨다운/충전 기준으로 한 번 이상 쓰는지
		bool bCooldownAvailable = false;

		// bChargeInfoAvailable = true일때만 유효
		bool bChargeInfoAvailable = false;
		int32 MaxCharges = 1;
		float CooldownDuration = 0.f;
	};

	struct FSkillSlotPresentationState
	{
		// 사용 가능한 충전이 하나 이상 있는지
		bool bAvailable = false;
		// 모든 충전이 회복된 상태인지
		bool bFullyCharged = false;
		// CooldownProgress를 표시할 수 있는지
		bool bProgressValid = false;

		float CooldownProgress = 0.f;
		float RemainingSeconds = 0.f;

		// false면 충전 개수 값을 사용하지 않음 -> 충전 점 그리지 않음
		bool bChargeCountKnown = false;
		int32 AvailableCharges = 0;
		int32 MaxCharges = 0;
	};

	enum class EAvailabilityVisual : uint8
	{
		Unknown,
		Unavailable,
		Available
	};

	// ══════════════════════════════════════════════════════════════
	//  표시: 취득 → 계산 → 적용   → ShooterSkillSlot_Display.cpp
	// ══════════════════════════════════════════════════════════════

	bool BuildStateFromDebug(FSkillSlotState& Out) const;
	bool BuildStateFromAbility(FSkillSlotState& Out) const;

	// 슬롯 상태로부터 UI 표시값 계산하고 위젯 상태는 변경하지 않는다.
	FSkillSlotPresentationState BuildPresentation(const FSkillSlotState& SlotState) const;

	// 표시 상태를 위젯에 적용
	void ApplyPresentation(const FSkillSlotPresentationState& Presentation);
	// 쿨다운 진행도와 남은 시간 화면에 반영
	void ApplyCooldownVisual(
		float RemainingSeconds, bool bProgressValid, float CooldownProgress);
	// 스킬 사용 가능 여부에 따른 시각 상태를 적용
	void ApplyAvailabilityVisual(bool bAvailable, bool bFullyCharged);

	// 완충 상태를 즉시 반영할지 결정
	void RequestFullChargeFinalize();
	// 완충 상태로 UI 결정
	void FinalizeFullCharge();

	UFUNCTION()
	void HandleReadyAnimFinished();

	// 갱신 창구. 트리거(Event / Debug OneShotTimer)가 있을 때만 불림
	void RefreshCooldownDisplay();

	void RebuildChargeBars(int32 InMaxCharges);


	// 매 갱신마다 재생성하지 않고 이 배열의 SetFilled만 호출한다
	UPROPERTY(Transient)
	TArray<TObjectPtr<UShooterChargeBar>> ChargeBars;

	int32 LastDisplayedTenths = INDEX_NONE;
	int32 LastSpentCharges = INDEX_NONE;

	// 로컬 시간 진행용 NativeTick은 이 두값만 읽고 ASC/디버그 배열은 안건드림
	float RecoveryEndWorldTime = 0.f; // 0이면 READY(진행 중 회복 없음)
	float DisplayCooldownDuration = 0.f;

	// Unknown 이면 아직 한 번도 적용 안 함. 최초 적용에는 Ready 연출을 내지 않는다
	EAvailabilityVisual LastAvailabilityVisual = EAvailabilityVisual::Unknown;
	// Anim_Ready 재생 중이면 완충 정리를 애니메이션 종료까지 미룬다
	bool bFullChargeFinalizePending = false;

	// ══════════════════════════════════════════════════════════════
	//  배관: 태그 바인딩 · 트리거 · 연출   → ShooterSkillSlot.cpp
	// ══════════════════════════════════════════════════════════════

	// InventoryRetryAttemptsLeft가 남아있는 동안 TryBindCooldownTag를 짧게 재시도
	void AttemptBindCooldownTag();

	// CachedCooldownTag를 아직 못 찾았으면 SlotInputTag로 탐색 시도
	void TryBindCooldownTag(URogueAbilitySystemComponent* ASC);

	UFUNCTION()
	void HandleInventoryChanged();
	void HandleCooldownTagChanged(FGameplayTag Tag, int32 NewCount);

	// 태그 개수 변화 없이 쿨타임 시간만 바뀐 경우 (냉각 장치 등)
	void HandleCooldownTimeChanged(FGameplayTag Tag);

	// 충전 완료시 연출
	void PlayReadyGlow();

	UFUNCTION()
	void HandleGlowReadyAnimFinished();

	// Cooldown시 실패 피드백위함
	UFUNCTION()
	void HandleAbilityFailed(
		const UGameplayAbility* Ability,
		const FGameplayTagContainer& FailureReason);

	// ── 배관 상태 값 ──

	// 실데이터 이벤트 구독 상태
	FGameplayTag CachedCooldownTag;
	FDelegateHandle CooldownTagEventHandle;
	FDelegateHandle AbilityFailedEventHandle;
	FDelegateHandle CooldownTimeChangedHandle;
	TWeakObjectPtr<UAugmentInventoryComponent> BoundInventory;

	// bounded retry 이 횟수만큼 짧게 재확인하고 멈춤
	static constexpr int32 MaxInventoryRetryAttempts = 5;
	int32 InventoryRetryAttemptsLeft = 0;
	FTimerHandle InventoryRetryTimerHandle;

	// 각 항목이 충전 하나가 회복되는 월드 시각
	TArray<float> DebugPendingRecoveryTimes;
	FTimerHandle DebugNextRecoveryTimerHandle;

	// ══════════════════════════════════════════════════════════════
	//  머티리얼 MID
	// ══════════════════════════════════════════════════════════════

	UMaterialInstanceDynamic* GetIconMID();     // _Icon.cpp Image_Icon 브러시
	UMaterialInstanceDynamic* GetDigitMID();    // _Display.cpp Text_Cooldown 폰트
	UMaterialInstanceDynamic* GetGlitchMID();   // 본체 RetainerBox 이펙트
	UMaterialInstanceDynamic* GetKeyFillMID();  // Key Frame
	UMaterialInstanceDynamic* GetKeyLabelMID(); // Key 폰트

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CachedIconMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CachedDigitMID;
	// 런타임 파라미터 제어에 사용
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CachedGlitchMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CachedKeyFillMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CachedKeyLabelMID;
};
