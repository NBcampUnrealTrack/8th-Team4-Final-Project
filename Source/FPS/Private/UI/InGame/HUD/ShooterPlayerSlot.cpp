// ShooterPlayerSlot.cpp
#include "UI/InGame/HUD/ShooterPlayerSlot.h"

#include "Components/Image.h"
#include "GameMode/BaseRoundGameState.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/GameplayTags.h"
#include "UI/Common/ShooterGauge.h"

UShooterPlayerSlot::UShooterPlayerSlot()
{
	// 대상을 컨테이너가 나중에 지정. 로컬 플레이어에 자동을 붙기 방지
	bAutoBindToOwningPlayer = false;
}

void UShooterPlayerSlot::NativeDestruct()
{
	UnbindFromPlayerState();

	Super::NativeDestruct();
}

void UShooterPlayerSlot::OnAbilitySystemReady(URogueAbilitySystemComponent* ASC)
{
	Super::OnAbilitySystemReady(ASC);

	if (bShowHealth)
	{
		ASC->GetGameplayAttributeValueChangeDelegate(
			URogueVitalAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HandleHealthChanged);
		ASC->GetGameplayAttributeValueChangeDelegate(
			URogueVitalAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::HandleMaxHealthChanged);

		CachedHealth = ASC->GetNumericAttribute(URogueVitalAttributeSet::GetHealthAttribute());
		CachedMaxHealth = ASC->GetNumericAttribute(URogueVitalAttributeSet::GetMaxHealthAttribute());

		const float TargetHealthRatio = (CachedMaxHealth > 0.f) ? CachedHealth / CachedMaxHealth : 0.f;

		Gauge_Health->SetProgressImmediate(TargetHealthRatio);
	}

	// 아군/적군 관계없이 폭탄 보유 태그는 항상 구독, 실제로 뱃지를 보여줄지는 RefreshBombCarrierIcon에서 공격팀 여부로 gate.
	BombCarryingTagEventHandle = ASC->RegisterGameplayTagEvent(
		RogueTags::State_Combat_DataJack_Carrying, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleBombCarryingTagChanged);

	bIsBombCarrier = ASC->HasMatchingGameplayTag(RogueTags::State_Combat_DataJack_Carrying);
	RefreshBombCarrierIcon();
}

void UShooterPlayerSlot::OnAbilitySystemLost(URogueAbilitySystemComponent* ASC)
{
	Super::OnAbilitySystemLost(ASC);

	ASC->GetGameplayAttributeValueChangeDelegate(
	URogueVitalAttributeSet::GetHealthAttribute()).RemoveAll(this);
	ASC->GetGameplayAttributeValueChangeDelegate(
		URogueVitalAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);

	//  폭탄 보유 태그 구독 해제
	if (BombCarryingTagEventHandle.IsValid())
	{
		ASC->UnregisterGameplayTagEvent(
			BombCarryingTagEventHandle, RogueTags::State_Combat_DataJack_Carrying, EGameplayTagEventType::NewOrRemoved);
		BombCarryingTagEventHandle.Reset();
	}

	bIsBombCarrier = false;
	RefreshBombCarrierIcon();
}

void UShooterPlayerSlot::UnbindFromPlayerState()
{
	if (BoundPlayerState.IsValid())
	{
		BoundPlayerState->OnPlayerAliveStateChanged.RemoveAll(this);
	}

	BoundPlayerState.Reset();
}

void UShooterPlayerSlot::SetPlayerState(AShooterPlayerState* NewPlayerState)
{
	UnbindFromPlayerState();

	BoundPlayerState = NewPlayerState;

	// 비우기 요청이면 숨기고 끝낸다.
	if (NewPlayerState == nullptr)
	{
		SetSourcePlayerState(nullptr);
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	NewPlayerState->OnPlayerAliveStateChanged.AddDynamic(this, &ThisClass::HandleAliveStateChanged);

	// 현재 생사 즉시 반영
	HandleAliveStateChanged(NewPlayerState->IsAlive());

	// ShooterAttributeWidget에서 구독
	SetSourcePlayerState(NewPlayerState);
}

void UShooterPlayerSlot::SetSlotStyle(bool bInShowHealth, FLinearColor InTintColor)
{
	bShowHealth = bInShowHealth;
	TintColor = InTintColor;

	if (Image_AliveIcon)
	{
		Image_AliveIcon->SetColorAndOpacity(TintColor);
	}

	if (Image_DeadIcon)
	{
		Image_DeadIcon->SetColorAndOpacity(TintColor);
	}

	if (Gauge_Health)
	{
		Gauge_Health->SetVisibility(bShowHealth ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UShooterPlayerSlot::RefreshHealth()
{
	if (CachedMaxHealth > 0.f && Gauge_Health)
	{
		const float TargetHealthRatio = (CachedMaxHealth > 0.f) ? CachedHealth / CachedMaxHealth : 0.f;

		Gauge_Health->SetProgress(TargetHealthRatio);
	}
}

void UShooterPlayerSlot::HandleAliveStateChanged(bool bNewIsAlive)
{
	if (Image_AliveIcon && Image_DeadIcon)
	{
		Image_AliveIcon->SetVisibility(bNewIsAlive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);

		Image_DeadIcon->SetVisibility(bNewIsAlive ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UShooterPlayerSlot::HandleHealthChanged(const struct FOnAttributeChangeData& Data)
{
	CachedHealth = Data.NewValue;
	RefreshHealth();
}

void UShooterPlayerSlot::HandleMaxHealthChanged(const struct FOnAttributeChangeData& Data)
{
	CachedMaxHealth = Data.NewValue;
	RefreshHealth();
}

// 폭탄(Data Jack) 보유자 뱃지
void UShooterPlayerSlot::HandleBombCarryingTagChanged(FGameplayTag Tag, int32 NewCount)
{
	bIsBombCarrier = NewCount > 0;
	RefreshBombCarrierIcon();
}

void UShooterPlayerSlot::RefreshBombCarrierIcon()
{
	if (!Image_BombCarrierIcon)
	{
		return;
	}

	const bool bShouldShow = bIsBombCarrier && IsLocalTeamAttacking();
	Image_BombCarrierIcon->SetVisibility(bShouldShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
}

bool UShooterPlayerSlot::IsLocalTeamAttacking() const
{
	const ABaseTeamPlayerState* LocalPlayerState = GetOwningPlayerState<ABaseTeamPlayerState>();
	const UWorld* World = GetWorld();
	const ABaseRoundGameState* RoundGameState = World ? World->GetGameState<ABaseRoundGameState>() : nullptr;

	if (!IsValid(LocalPlayerState) || !IsValid(RoundGameState))
	{
		return false;
	}

	return RoundGameState->GetTeamRole(LocalPlayerState->GetTeam()) == ETeamRole::Attackers;
}
