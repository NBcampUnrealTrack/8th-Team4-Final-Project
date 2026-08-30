// ShooterSkillSlot_Display.cpp
#include "UI/InGame/HUD/ShooterSkillSlot.h"

#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/Common/ShooterGauge.h"
#include "Materials/MaterialInstanceDynamic.h"

auto UShooterSkillSlot::BuildPresentation(const FSkillSlotState& SlotState) const
	-> FSkillSlotPresentationState
{
	FSkillSlotPresentationState Out;

	Out.bAvailable = SlotState.bCooldownAvailable;
	Out.bFullyCharged = (SlotState.SpentCharges == 0);
	Out.RemainingSeconds = SlotState.NextRecoveryRemaining;

	Out.bProgressValid = (SlotState.CooldownDuration > 0.f);
	if (Out.bProgressValid)
	{
		Out.CooldownProgress =
			FMath::Clamp(1.f - (SlotState.NextRecoveryRemaining / SlotState.CooldownDuration), 0.f, 1.f);
	}

	Out.bChargeCountKnown = SlotState.bChargeInfoAvailable;
	if (Out.bChargeCountKnown)
	{
		Out.MaxCharges = SlotState.MaxCharges;
		Out.AvailableCharges = FMath::Max(0, SlotState.MaxCharges - SlotState.SpentCharges);
	}

	return Out;
}

void UShooterSkillSlot::ApplyPresentation(const FSkillSlotPresentationState& Presentation)
{
	ApplyOccupiedState();
	ApplyAvailabilityVisual(Presentation.bAvailable, Presentation.bFullyCharged);

	if (Presentation.bChargeCountKnown)
	{
		SetCharges(Presentation.AvailableCharges, Presentation.MaxCharges);
	}
	else if (HBox_ChargeBars)
	{
		HBox_ChargeBars->SetVisibility(ESlateVisibility::Hidden);
	}

	if (Presentation.bFullyCharged)
	{
		SetReady();
	}
	else
	{
		ApplyCooldownVisual(Presentation.RemainingSeconds, Presentation.bProgressValid, Presentation.CooldownProgress);
	}
}

void UShooterSkillSlot::ApplyCooldownVisual(float RemainingSeconds, bool bProgressValid, float CooldownProgress)
{
	// 이전 완충 연출의 지연 정리가 현재 쿨다운 표시를 덮지 않도록 취소
	bFullChargeFinalizePending = false;

	if (!Gauge_Cooldown || !Text_Cooldown)
	{
		return;
	}
	if (Image_ReadyGlow)
	{
		Image_ReadyGlow->SetVisibility(ESlateVisibility::Collapsed);
	}
	Text_Cooldown->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (bProgressValid)
	{
		Gauge_Cooldown->SetProgressImmediate(CooldownProgress);
		if (UMaterialInstanceDynamic* MID = GetDigitMID())
		{
			MID->SetScalarParameterValue(TEXT("Progress"), CooldownProgress);
		}
		if (UMaterialInstanceDynamic* MID = GetIconMID())
		{
			MID->SetScalarParameterValue(TEXT("Progress"), CooldownProgress);
		}
	}

	const int32 DisplaySeconds = FMath::CeilToInt(FMath::Max(0.f, RemainingSeconds));

	Text_Cooldown->SetText(FText::AsNumber(DisplaySeconds));
}

void UShooterSkillSlot::ApplyAvailabilityVisual(bool bAvailable, bool bFullyCharged)
{
	LastAvailabilityVisual = bAvailable ? EAvailabilityVisual::Available : EAvailabilityVisual::Unavailable;

	if (!bFullyCharged && Anim_Ready)
	{
		StopAnimation(Anim_Ready);
	}

	const bool bReadyAnimationOwnsIcon =
		bFullyCharged &&
		Anim_Ready &&
		IsAnimationPlaying(Anim_Ready);
	
	if (!bReadyAnimationOwnsIcon)
	{
		const float IconReadiness = bFullyCharged ? 1.f : 0.f;
		
		if (UMaterialInstanceDynamic* MID = GetIconMID())
		{
			MID->SetScalarParameterValue(TEXT("Readiness"), IconReadiness);
		}
	}
	
	const float InputReadiness = bAvailable ? 1.f : 0.f;

	if (UMaterialInstanceDynamic* MID = GetKeyFillMID())
	{
		MID->SetScalarParameterValue(TEXT("Progress"), InputReadiness);
	}

	if (UMaterialInstanceDynamic* MID = GetKeyLabelMID())
	{
		MID->SetScalarParameterValue(TEXT("Progress"), InputReadiness);
	}
}

void UShooterSkillSlot::RequestFullChargeFinalize()
{
	if (Anim_Ready && IsAnimationPlaying(Anim_Ready))
	{
		bFullChargeFinalizePending = true;
		return;
	}
	FinalizeFullCharge();
}

void UShooterSkillSlot::FinalizeFullCharge()
{
	bFullChargeFinalizePending = false;

	if (Text_Cooldown)
	{
		Text_Cooldown->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Gauge_Cooldown)
	{
		Gauge_Cooldown->SetProgressImmediate(0.f);
	}
	LastDisplayedTenths = INDEX_NONE;
}

void UShooterSkillSlot::HandleReadyAnimFinished()
{
	if (LastAvailabilityVisual == EAvailabilityVisual::Available)
	{
		if (UMaterialInstanceDynamic* MID = GetIconMID())
		{
			MID->SetScalarParameterValue(TEXT("Readiness"), 1.f);
		}
	}

	if (bFullChargeFinalizePending)
	{
		FinalizeFullCharge();
	}
}
