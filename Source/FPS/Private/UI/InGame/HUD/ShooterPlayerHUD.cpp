// ShooterPlayerHUD.cpp
#include "UI/InGame/HUD/ShooterPlayerHUD.h"
#include "Components/Image.h"
#include "UI/InGame/HUD/ShooterHealthBar.h"
#include "Animation/WidgetAnimation.h"
#include "Components/RetainerBox.h"

UShooterPlayerHUD::UShooterPlayerHUD()
{
	InputConfig = EShooterWidgetInputMode::Game;
}

void UShooterPlayerHUD::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	if (!HealthBar)
	{
		UE_LOG(LogTemp, Error, TEXT("[ShooterPlayerHUD] ShooterHealthBar가 지정되지않았습니다."));
		return;
	}
	
	if (Image_LowHealthVignette)
	{
		Image_LowHealthVignette->SetVisibility(ESlateVisibility::Hidden);
	}
	
	// 초기화 시 LowHealth 상태 확인을 위함
	HandleLowHealthStateChanged(HealthBar->IsLowHealth());
	
	HealthBar->OnHealthDamaged.AddUObject(this, &ThisClass::HandleHealthDamaged);
	HealthBar->OnShieldDamaged.AddUObject(this, &ThisClass::HandleShieldDamaged);
	HealthBar->OnLowHealthStateChanged.AddUObject(this, &ThisClass::HandleLowHealthStateChanged);
	
	if (Anim_LowHealth)
	{
		FWidgetAnimationDynamicEvent FinishedLowAnim;
		FinishedLowAnim.BindDynamic(this, &ThisClass::HandleLowHealthAnimationFinished);
		BindToAnimationFinished(Anim_LowHealth, FinishedLowAnim);
	}
}

void UShooterPlayerHUD::NativeConstruct()
{
	Super::NativeConstruct();
	
	// Retainer Box의 Effect Material MID를 캐싱
	if (RetainerBox_HUD)
	{
		HUDGlitchMID = RetainerBox_HUD->GetEffectMaterial();
		
		if (HUDGlitchMID)
		{
			HUDGlitchMID->SetScalarParameterValue(TEXT("GlitchStrength"), 0.0f);
		}
	}
}

void UShooterPlayerHUD::NativeDestruct()
{
	if (HealthBar)
	{
		HealthBar->OnHealthDamaged.RemoveAll(this);
		HealthBar->OnShieldDamaged.RemoveAll(this);
		HealthBar->OnLowHealthStateChanged.RemoveAll(this);
	}
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LowHealthGlitchTimerHandle);
	}
	
	Super::NativeDestruct();
}

void UShooterPlayerHUD::ScheduleNextLowHealthGlitch()
{
	if (!bIsLowHealthActive || !GetWorld())
	{
		return;
	}
	
	const float RandomDelay = FMath::FRandRange(RandomGlitchMin, RandomGlitchMax);
	
	GetWorld()->GetTimerManager().SetTimer(
		LowHealthGlitchTimerHandle,
		this,
		&ThisClass::PlayLowHealthGlitchPulse,
		RandomDelay,
		false
	);
}

void UShooterPlayerHUD::PlayLowHealthGlitchPulse()
{
	if (HUDGlitchMID)
	{
		HUDGlitchMID->SetScalarParameterValue(TEXT("GlitchStrength"), 1.0f);
	}
	
	if (Anim_LowHealth)
	{
		PlayAnimation(Anim_LowHealth);
	}
	
	ScheduleNextLowHealthGlitch();
}

void UShooterPlayerHUD::HandleHealthDamaged()
{
	if (Anim_HitFlash_Health)
	{
		PlayAnimation(Anim_HitFlash_Health);
	}
}

void UShooterPlayerHUD::HandleShieldDamaged()
{
	if (Anim_HitFlash_Shield)
	{
		PlayAnimation(Anim_HitFlash_Shield);
	}
}

void UShooterPlayerHUD::HandleLowHealthStateChanged(bool bIsLow)
{
	bIsLowHealthActive = bIsLow;
	
	if (Anim_LowHealth)
	{
		if (bIsLow)
		{
			if (Image_LowHealthVignette)
			{
				Image_LowHealthVignette->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			ScheduleNextLowHealthGlitch();
		}
		else
		{
			if (Image_LowHealthVignette)
			{
				Image_LowHealthVignette->SetVisibility(ESlateVisibility::Hidden);
			}

			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(LowHealthGlitchTimerHandle);
			}
		}
	}
}

void UShooterPlayerHUD::HandleLowHealthAnimationFinished()
{
	if (HUDGlitchMID)
	{
		HUDGlitchMID->SetScalarParameterValue(TEXT("GlitchStrength"), 0.0f);
	}
}
