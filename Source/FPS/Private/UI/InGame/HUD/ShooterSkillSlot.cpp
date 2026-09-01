// ShooterSkillSlot.cpp
#include "UI/InGame/HUD/ShooterSkillSlot.h"

#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Augment/AugmentInventoryComponent.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "Player/ShooterPlayerState.h"
#include "UI/InGame/HUD/ShooterChargeBar.h"
#include "UI/Common/ShooterGauge.h"
#include "CommonTextBlock.h"
#include "TimerManager.h"
#include "GameplayEffectTypes.h"
#include "Components/RetainerBox.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterSkillSlot, Log, All);

void UShooterSkillSlot::NativeOnInitialized()
{
	Super::NativeOnInitialized(); // ASC가 이미 있으면 OnAbilitySystemReady 호출
	
	if (Anim_ReadyGlow)
	{
		FWidgetAnimationDynamicEvent FinishedReadyAnim;
		FinishedReadyAnim.BindDynamic(this, &ThisClass::HandleGlowReadyAnimFinished);
		BindToAnimationFinished(Anim_ReadyGlow, FinishedReadyAnim);
	}
	
	if (!bUseDebugDriver)
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
	
	// 디버그 모드는 ASC 이벤트를 안 받으므로 초기 Ready 상태를 여기서 한 번 잡아준다.
	// 실데이터 모드에서도 상관없다 CachedCooldownTag가 없으면 리턴
	RefreshCooldownDisplay();
}

void UShooterSkillSlot::NativePreConstruct()
{
	Super::NativePreConstruct();
	
	if (Text_KeyLabel)
	{
		Text_KeyLabel->SetText(KeyLabel);
	}
}

void UShooterSkillSlot::NativeDestruct()
{
	// 실데이터 구독 해제는 OnAbilitySystemLost에서 한다
	// Super::NativeDestruct() 베이스에가 그 함수를 리셋전 불러줌
	if (UWorld* World =GetWorld())
	{
		World->GetTimerManager().ClearTimer(DebugNextRecoveryTimerHandle);
	}
	
	Super::NativeDestruct();
}

void UShooterSkillSlot::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	// Ready 상태(진행 중인 회복 없음)
	if (RecoveryEndWorldTime <= 0)
	{
		return;
	}
	
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	const float Remaining = FMath::Max(0.f, RecoveryEndWorldTime - World->GetTimeSeconds());
	SetCooldownState(Remaining, DisplayCooldownDuration);
}

void UShooterSkillSlot::OnAbilitySystemReady(URogueAbilitySystemComponent* ASC)
{
	Super::OnAbilitySystemReady(ASC);
	
	// Debug/Real 경로를 완전 분리한다 
	if (bUseDebugDriver)
	{
		UE_LOG(LogShooterSkillSlot, Warning, TEXT("Debug 상태입니다. bUseDebugDriver = true"));
		return;
	}
	
	// ASC가 준비되는 과정과 UI가 ASC를 사용하는 과정 사이에 순서 보장이 없을 수 있다
	// 그래서 여기도 bounded retry
	InventoryRetryAttemptsLeft = MaxInventoryRetryAttempts;
	AttemptBindCooldownTag();
	
	// 증강을 아직 안 뽑았으면 못 찾고 끝나기때문에 
	// 인벤토리가 바뀔때 마다(= 증강을 새로 뽑을때마다) 다시 시도하도록 구독
	if (AShooterPlayerState* PS = GetOwningPlayerState<AShooterPlayerState>())
	{
		if (UAugmentInventoryComponent* Inventory = PS->GetAugmentInventory())
		{
			BoundInventory = Inventory;
			Inventory->OnInventoryChanged.AddDynamic(this, &ThisClass::HandleInventoryChanged);
		}
	}
}

void UShooterSkillSlot::OnAbilitySystemLost(URogueAbilitySystemComponent* ASC)
{
	if (ASC && CooldownTagEventHandle.IsValid())
	{
		ASC->UnregisterGameplayTagEvent(CooldownTagEventHandle, CachedCooldownTag, EGameplayTagEventType::AnyCountChange);
		CooldownTagEventHandle.Reset();
	}
	
	if (ASC && AbilityFailedEventHandle.IsValid())
	{
		ASC->AbilityFailedCallbacks.Remove(AbilityFailedEventHandle);
		AbilityFailedEventHandle.Reset();
	}
	
	if (BoundInventory.IsValid())
	{
		BoundInventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
		BoundInventory.Reset();
	}
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InventoryRetryTimerHandle);
	}
	
	Super::OnAbilitySystemLost(ASC);
}

void UShooterSkillSlot::HandleGlowReadyAnimFinished()
{
	if (CachedGlitchMID)
	{
		CachedGlitchMID->SetScalarParameterValue(TEXT("GlitchStrength"), 0.0f);
	}
}

void UShooterSkillSlot::HandleAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason)
{
	if (!CachedCooldownTag.IsValid() || !FailureReason.HasTagExact(CachedCooldownTag))
	{
		return;
	}
	
	if (Anim_CooldownReject)
	{
		PlayAnimation(Anim_CooldownReject);
	}
	
	// TODO: 사운드
}

void UShooterSkillSlot::SetCooldownState(float RemainingSeconds, float TotalCooldown)
{
	if (!Gauge_Cooldown || !Text_Cooldown)
	{
		return;
	}
	
	if (Image_ReadyGlow)
	{
		Image_ReadyGlow->SetVisibility(ESlateVisibility::Collapsed);
	}
	
	Text_Cooldown->SetVisibility(ESlateVisibility::HitTestInvisible);
	
	// 스킬 쿨타임 시각화 표시 위한 Alpha. 경과 비율 = 1 - (남은시간/전체시간)
	if (TotalCooldown > 0.f)
	{
		const float ElapsedAlpha = 1.f - (RemainingSeconds / TotalCooldown);
		Gauge_Cooldown->SetProgressImmediate(FMath::Clamp(ElapsedAlpha, 0.f, 1.f));
	}
	
	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = 1;
	Options.MaximumFractionalDigits = 1;

	Text_Cooldown->SetText(FText::AsNumber(FMath::Max(0.f, RemainingSeconds), &Options));
}

void UShooterSkillSlot::SetCharges(int32 AvailableCharges, int32 InMaxCharges)
{
	const int32 MaxCharges = FMath::Max(1, InMaxCharges);
	const int32 Available = FMath::Clamp(AvailableCharges, 0, MaxCharges);
	
	RebuildChargeBars(MaxCharges);
	
	for (int32 Index = 0; Index < ChargeBars.Num(); ++Index)
	{
		if (ChargeBars[Index])
		{
			ChargeBars[Index]->SetFilled(Index < Available);
		}
	}
	
	// 사용 가능 충전이 0 -> 1 이상으로 늘어난 다시 쓸 수 있게 된 순간만 체크
	// 최대치까지 다 안 찼어도 Animation 연출을 재생해야 충전형 스킬의 피드백이 된다.
	if (LastAvailableCharges >= 0 && Available > LastAvailableCharges)
	{
		PlayReadyGlow();
	}
	
	LastAvailableCharges = Available;
}

void UShooterSkillSlot::SetReady()
{
	// 진행 중인 쿨다운이 없으면 게이지/텍스트 숨김
	// Glow는 SetCharges에서
	if (Text_Cooldown)
	{
		Text_Cooldown->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 다음 쿨다운 시작될 때 
	LastDisplayedTenths = INDEX_NONE;
	
	if (Gauge_Cooldown)
	{
		Gauge_Cooldown->SetProgressImmediate(1.f);
	}
	
	// 충전 연출 재생
	PlayReadyGlow();
}

void UShooterSkillSlot::Debug_ConsumeCharge()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	// 충전이 남아있지 않으면 캐스트 자체가 안됨
	if (DebugPendingRecoveryTimes.Num() >= Debug_MaxCharges)
	{
		return;
	}
	
	const float Now = World->GetTimeSeconds();
	
	const float StartFrom = DebugPendingRecoveryTimes.Num() > 0 ? DebugPendingRecoveryTimes.Last() : Now;
	
	DebugPendingRecoveryTimes.Add(StartFrom + Debug_CooldownSeconds);
	
	RefreshCooldownDisplay();
}

bool UShooterSkillSlot::BuildStateFromDebug(FSkillSlotState& Out) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	
	const float Now = World->GetTimeSeconds();
	
	// 만료된 항목 제거 = 충전회복 상태. 배열이 시간 오름차순이라 앞에서부터 걷어낸다.
	TArray<float>& Pending = const_cast<TArray<float>&>(DebugPendingRecoveryTimes);
	while (Pending.Num() > 0 && Pending[0] <= Now)
	{
		Pending.RemoveAt(0);
	}
	
	Out.SpentCharges = Pending.Num();
	
	// 디버그 경로는 규칙을 스스로 정의하므로 충전 정보를 앎
	Out.bChargeInfoAvailable = true;
	Out.MaxCharges = FMath::Max(1, Debug_MaxCharges);
	Out.CooldownDuration = Debug_CooldownSeconds;
	
	Out.NextRecoveryRemaining = Pending.Num() > 0 ? (Pending[0] - Now) : 0.f;
	
	return true;
}

bool UShooterSkillSlot::BuildStateFromAbility(FSkillSlotState& Out) const
{
	URogueAbilitySystemComponent* ASC = BoundASC.Get();
	// CachedCooldownTag가 없으면 증강을 뽑지않았음 판정
	if (!ASC || !CachedCooldownTag.IsValid())
	{
		return false;
	}
	
	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(CachedCooldownTag);
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags);
	
	Out.SpentCharges = ASC->GetAggregatedStackCount(Query);
	
	if (Out.SpentCharges <= 0)
	{
		return true;
	}
	
	// TODO:
	// 실제 다충전 HUD는 Gameplay 쪽에서
	// MaxCharges / ChargeCooldownDuration을 제공한 뒤 연결한다.
	// 현재 다충전 Bar/Gauge 동작은 Debug Driver에서만 검증한다.
	const TArray<TPair<float, float>> Times = ASC->GetActiveEffectsTimeRemainingAndDuration(Query);
	
	if (Times.IsEmpty())
	{
		return false;
	}
	
	float MinRemaining = TNumericLimits<float>::Max();
	float DurationAtMin = 0.f;
	
	for (const TPair<float, float>& Pair : Times)
	{
		if (Pair.Key < MinRemaining)
		{
			MinRemaining = Pair.Key;
			DurationAtMin = Pair.Value;
		}
		MinRemaining = FMath::Min(MinRemaining, Pair.Key);
	}
	
	Out.NextRecoveryRemaining = FMath::Max(0.f, MinRemaining);
	Out.CooldownDuration = DurationAtMin;
	
	return true;
}

void UShooterSkillSlot::RefreshCooldownDisplay()
{
	FSkillSlotState SlotState;
	
	const bool bOk = bUseDebugDriver ? BuildStateFromDebug(SlotState) : BuildStateFromAbility(SlotState);
	
	// 디버그시만 통과
	if (!bOk)
	{
		return;
	}
	
	// MaxCharges를 아는 경로에서만 Bar/Ready를 그린다.
	if (SlotState.bChargeInfoAvailable)
	{
		SetCharges(SlotState.MaxCharges - SlotState.SpentCharges, SlotState.MaxCharges);
	}
	else if (HBox_ChargeBars)
	{
		HBox_ChargeBars->SetVisibility(ESlateVisibility::Hidden);
	}
	
	if (SlotState.SpentCharges == 0)
	{
		SetReady();
		RecoveryEndWorldTime = 0.f;
	}
	else
	{
		SetCooldownState(SlotState.NextRecoveryRemaining, SlotState.CooldownDuration);
		
		if (const UWorld* World = GetWorld())
		{
			RecoveryEndWorldTime = World->GetTimeSeconds() + SlotState.NextRecoveryRemaining;
			DisplayCooldownDuration = SlotState.CooldownDuration;
		}
	}
	
	// 디버그 경로는 이벤트가 없으므로 다음충전이 회복되는 시각에만 재호출
	// 실데이터는 타이머 사용하지않고 RegisterGameplayTagEvent가 대신 불러준다.
	if (bUseDebugDriver)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DebugNextRecoveryTimerHandle);
			if (SlotState.SpentCharges > 0)
			{
				World->GetTimerManager().SetTimer(
					DebugNextRecoveryTimerHandle, 
					this, 
					&ThisClass::RefreshCooldownDisplay, 
					FMath::Max(0.01f, SlotState.NextRecoveryRemaining),
					false);
			}
		}
	}
}

void UShooterSkillSlot::RebuildChargeBars(int32 InMaxCharges)
{
	if (!HBox_ChargeBars || !ChargeBarWidgetClass || ChargeBars.Num() == InMaxCharges)
	{
		return; 
	}
	
	HBox_ChargeBars->ClearChildren();
	ChargeBars.Reset();
	
	for (int32 Index = 0; Index < InMaxCharges; ++Index)
	{
		UShooterChargeBar* Bar = CreateWidget<UShooterChargeBar>(this, ChargeBarWidgetClass);
		if (!Bar)
		{
			continue;
		}
		
		ChargeBars.Add(Bar);
		if (UHorizontalBoxSlot* BoxSlot = HBox_ChargeBars->AddChildToHorizontalBox(Bar))
		{
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BoxSlot->SetHorizontalAlignment(HAlign_Fill);
			BoxSlot->SetVerticalAlignment(VAlign_Fill);
			if (Index < InMaxCharges - 1)
			{
				BoxSlot->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
			}
		}
	}
}

void UShooterSkillSlot::AttemptBindCooldownTag()
{
	if (URogueAbilitySystemComponent* ASC = BoundASC.Get())
	{
		TryBindCooldownTag(ASC);
	}
	else
	{
		UE_LOG(LogShooterSkillSlot, Warning, TEXT("ASC가 없습니다."));
	}
	
	// 찾았거나 재시도할 여유가 없으면 여기서 멈춤
	// 못 찾으면 다음 OnInventoryChanged(= 다음 증강 획득)가 올 때까지 대기한다
	// 이 슬롯의 증강은 이미 뽑았는데 Spec 복제만 늦는 상황 메우는 용도
	if (CachedCooldownTag.IsValid() || InventoryRetryAttemptsLeft <= 0)
	{
		// UE_LOG(LogShooterSkillSlot, Warning, TEXT("찾은 CooldownTag : %s"), *CachedCooldownTag.ToString());
		return;
	}
	
	--InventoryRetryAttemptsLeft;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(InventoryRetryTimerHandle, this, &ThisClass::AttemptBindCooldownTag, 0.15f, false);
	}
}

void UShooterSkillSlot::TryBindCooldownTag(URogueAbilitySystemComponent* ASC)
{
	if (!ASC || !SlotInputTag.IsValid() || CachedCooldownTag.IsValid())
	{
		return;
	}
	
	// Spec 탐색은 여기서 단 한번
	const FGameplayAbilitySpec* FoundSpec = nullptr;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(SlotInputTag))
		{
			FoundSpec = &Spec;
			break;
		}
	}
	
	// 아직 못 찾으면 AttemptBindCooldownTag의 bounded retry 또는 다음 HandleInventoryChanged에서 재시도
	if (!FoundSpec)
	{
		return;
	}
	
	const FGameplayTagContainer* CooldownTags = FoundSpec->Ability->GetCooldownTags();
	if (!CooldownTags || CooldownTags->IsEmpty())
	{
		return;
	}
	
	CachedCooldownTag = CooldownTags->First();
	CooldownTagEventHandle = ASC->RegisterGameplayTagEvent(CachedCooldownTag, EGameplayTagEventType::AnyCountChange).AddUObject(this, &ThisClass::HandleCooldownTagChanged);
	AbilityFailedEventHandle = ASC->AbilityFailedCallbacks.AddUObject(this, &ThisClass::HandleAbilityFailed);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	
	// 늦게 발견 됐을 경우 이미 쿨다운 중일 수 있으므로 즉시 한번 반영
	RefreshCooldownDisplay(); 
}

void UShooterSkillSlot::PlayReadyGlow()
{
	if (Anim_ReadyGlow)
	{
		StopAnimation(Anim_ReadyGlow);
		PlayAnimation(Anim_ReadyGlow, 0.f, 1);

		if (UMaterialInstanceDynamic* MID = GetGlitchMID())
		{
			MID->SetScalarParameterValue(TEXT("GlitchStrength"), 1.0f);
		}
	}
	
	if (Image_ReadyGlow)
	{
		Image_ReadyGlow->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UShooterSkillSlot::HandleInventoryChanged()
{
	// 이미 태그를 찾아 구독 중이면 다시 찾지 않는다.
	if (CachedCooldownTag.IsValid())
	{
		return;
	}
	
	InventoryRetryAttemptsLeft = MaxInventoryRetryAttempts;
	AttemptBindCooldownTag();
}

void UShooterSkillSlot::HandleCooldownTagChanged(FGameplayTag Tag, int32 NewCount)
{
	RefreshCooldownDisplay();
}

UMaterialInstanceDynamic* UShooterSkillSlot::GetGlitchMID()
{
	if (!CachedGlitchMID && RetainerBox_Glitch)
	{
		CachedGlitchMID = RetainerBox_Glitch->GetEffectMaterial();
	}
	
	return CachedGlitchMID;
}



