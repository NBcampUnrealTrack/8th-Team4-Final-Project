// ShooterPlayerHUD.cpp
#include "UI/InGame/HUD/ShooterPlayerHUD.h"
#include "Combat/CombatComponent.h"
#include "Components/Image.h"
#include "UI/InGame/HUD/ShooterHealthBar.h"
#include "Animation/WidgetAnimation.h"
#include "Components/RetainerBox.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

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
		MainHUDContentDefaultVisibility = RetainerBox_HUD->GetVisibility();
		bMainHUDDefaultVisibilityCached = true;
		
		if (HUDGlitchMID)
		{
			HUDGlitchMID->SetScalarParameterValue(TEXT("GlitchStrength"), 0.0f);
		}
		
		// 글리치가 없을 때는 RT 를 거치지 않는다
		RetainerBox_HUD->SetRetainRendering(false);
	}

	BoundPlayerController = GetOwningPlayer();
	if (APlayerController* Controller = BoundPlayerController.Get())
	{
		Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}
	BindToPawn(GetOwningPlayerPawn());
}

void UShooterPlayerHUD::NativeDestruct()
{
	if (APlayerController* Controller = BoundPlayerController.Get())
	{
		Controller->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}
	BoundPlayerController.Reset();
	UnbindCombatComponent();

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

void UShooterPlayerHUD::HandlePossessedPawnChanged(APawn*, APawn* NewPawn)
{
	BindToPawn(NewPawn);
}

void UShooterPlayerHUD::BindToPawn(APawn* Pawn)
{
	UCombatComponent* CombatComponent = IsValid(Pawn)
		? Pawn->FindComponentByClass<UCombatComponent>()
		: nullptr;
	if (BoundCombatComponent.Get() != CombatComponent)
	{
		UnbindCombatComponent();
		BoundCombatComponent = CombatComponent;
		if (CombatComponent)
		{
			CombatComponent->OnMainHUDSuppressionChanged.AddUniqueDynamic(
				this, &ThisClass::HandleMainHUDSuppressionChanged);
		}
	}
	ApplyMainHUDSuppression(CombatComponent && CombatComponent->ShouldSuppressMainHUDForADS());
}

void UShooterPlayerHUD::UnbindCombatComponent()
{
	if (UCombatComponent* CombatComponent = BoundCombatComponent.Get())
	{
		CombatComponent->OnMainHUDSuppressionChanged.RemoveDynamic(
			this,
			&ThisClass::HandleMainHUDSuppressionChanged);
	}
	BoundCombatComponent.Reset();
}

void UShooterPlayerHUD::HandleMainHUDSuppressionChanged(const bool bSuppressed)
{
	ApplyMainHUDSuppression(bSuppressed);
}

void UShooterPlayerHUD::ApplyMainHUDSuppression(const bool bSuppressed)
{
	// RetainerBox_HUD가 일반 인게임 HUD의 시각 루트다. 스코프/레티클처럼 ADS에서도 살아야 하는
	// 오버레이는 이 RetainerBox 바깥에 두면 별도 Visibility 바인딩 없이 그대로 유지된다.
	if (RetainerBox_HUD)
	{
		if (!bMainHUDDefaultVisibilityCached)
		{
			MainHUDContentDefaultVisibility = RetainerBox_HUD->GetVisibility();
			bMainHUDDefaultVisibilityCached = true;
		}

		RetainerBox_HUD->SetVisibility(
			bSuppressed ? ESlateVisibility::Collapsed : MainHUDContentDefaultVisibility);
	}

	// 저체력 비네트가 RetainerBox 바깥에 배치된 BP에서도 ADS 중 잔상으로 남지 않게 한다.
	if (Image_LowHealthVignette)
	{
		if (bSuppressed)
		{
			Image_LowHealthVignette->SetVisibility(ESlateVisibility::Hidden);
		}
		else
		{
			Image_LowHealthVignette->SetVisibility(
				bIsLowHealthActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
	}
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
	// 글리치 동안만 RT 로 그린다
	if (RetainerBox_HUD)
	{
		RetainerBox_HUD->SetRetainRendering(true);
	}
	
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
	
	if (RetainerBox_HUD)
	{
		RetainerBox_HUD->SetRetainRendering(false);
	}
}
