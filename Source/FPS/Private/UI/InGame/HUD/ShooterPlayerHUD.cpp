// ShooterPlayerHUD.cpp
#include "UI/InGame/HUD/ShooterPlayerHUD.h"
#include "Combat/CombatComponent.h"
#include "Components/Image.h"
#include "UI/InGame/HUD/ShooterHealthBar.h"
#include "Animation/WidgetAnimation.h"
#include "Components/RetainerBox.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Widgets/SWidget.h"

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
	UnbindMovement();

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
	BindMovement(Pawn);
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

void UShooterPlayerHUD::BindMovement(APawn* Pawn)
{
	ACharacter* Character = Cast<ACharacter>(Pawn);
	if (BoundCharacter.Get() == Character)
	{
		return;
	}

	UnbindMovement();
	BoundCharacter = Character;
	if (!Character)
	{
		return;
	}

	Character->MovementModeChangedDelegate.AddUniqueDynamic(this, &ThisClass::HandleMovementModeChanged);
	Character->LandedDelegate.AddUniqueDynamic(this, &ThisClass::HandleLanded);

	// 공중에서 스폰·빙의됐으면 지금 높이를 기준으로 삼는다
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (Movement && Movement->IsFalling())
	{
		TakeoffZ = Character->GetActorLocation().Z;
		EnsureMotionScheduled();
	}
}

void UShooterPlayerHUD::UnbindMovement()
{
	if (ACharacter* Character = BoundCharacter.Get())
	{
		Character->MovementModeChangedDelegate.RemoveDynamic(this, &ThisClass::HandleMovementModeChanged);
		Character->LandedDelegate.RemoveDynamic(this, &ThisClass::HandleLanded);
	}
	BoundCharacter.Reset();
	ResetMotion();
}

void UShooterPlayerHUD::HandleMovementModeChanged(ACharacter* Character, const EMovementMode PrevMovementMode, uint8)
{
	// 이동 재생 중에 모드가 바뀌어도 이륙 높이는 처음 뜬 곳으로 둔다
	const UCharacterMovementComponent* Movement = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
	if (Movement && Movement->IsFalling() && PrevMovementMode != MOVE_Falling && !Character->bClientUpdating)
	{
		TakeoffZ = Character->GetActorLocation().Z;
	}
	EnsureMotionScheduled();
}

void UShooterPlayerHUD::HandleLanded(const FHitResult&)
{
	const ACharacter* Character = BoundCharacter.Get();
	if (!IsValid(Character) || Character->bClientUpdating)
	{
		return;
	}

	// SetPostLandedPhysics 전이라 낙하 속도가 아직 남아 있다
	const float FallSpeed = FMath::Max(0.f, static_cast<float>(-Character->GetVelocity().Z));
	const float SpeedRange = FMath::Max(1.f, FullLandingSpeed - MinLandingSpeed);
	const float ImpactScale = FMath::Clamp((FallSpeed - MinLandingSpeed) / SpeedRange, 0.f, 1.f);

	MotionVelocity += LandingKickSpeed * ImpactScale;
	EnsureMotionScheduled();
}

void UShooterPlayerHUD::EnsureMotionScheduled()
{
	if (!Panel_HUDMotion || MotionTimerHandle.IsValid() || IsDesignTime() || !FSlateApplication::IsInitialized())
	{
		return;
	}

	// RetainerBox 가 ADS 로 접혀도 멈추지 않게 HUD 루트에 건다
	const TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
	if (!CachedWidget.IsValid())
	{
		return;
	}

	MotionTimerHandle = CachedWidget->RegisterActiveTimer(
		0.f,
		FWidgetActiveTimerDelegate::CreateUObject(this, &ThisClass::HandleMotionTimer));
}

void UShooterPlayerHUD::StopMotion()
{
	const TSharedPtr<FActiveTimerHandle> Handle = MotionTimerHandle.Pin();
	const TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
	if (Handle.IsValid() && CachedWidget.IsValid())
	{
		CachedWidget->UnRegisterActiveTimer(Handle.ToSharedRef());
	}
	MotionTimerHandle.Reset();
}

void UShooterPlayerHUD::ResetMotion()
{
	StopMotion();
	MotionOffset = 0.f;
	MotionVelocity = 0.f;
	TakeoffZ = 0.f;
	if (Panel_HUDMotion)
	{
		Panel_HUDMotion->SetRenderTranslation(FVector2D::ZeroVector);
	}
}

EActiveTimerReturnType UShooterPlayerHUD::HandleMotionTimer(double, const float InDeltaTime)
{
	const ACharacter* Character = BoundCharacter.Get();
	const UCharacterMovementComponent* Movement = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;
	const bool bAirborne = Movement && Movement->IsFalling();

	// 이륙 높이보다 올라간 만큼만 뜬다. 낭떠러지로 내려가면 0
	const float Height = bAirborne ? static_cast<float>(Character->GetActorLocation().Z - TakeoffZ) : 0.f;
	const float Target = -FMath::Clamp(Height * AirLiftPerCm, 0.f, MaxAirLift);

	const float DampingRatio = bAirborne ? 1.f : MotionDampingRatio;
	FMath::SpringDamper(MotionOffset, MotionVelocity, Target, 0.f, InDeltaTime, MotionFrequency, DampingRatio);

	// 공중에 있는 동안엔 높이가 계속 바뀌므로 땅에서만 멈춘다
	const bool bSettled = !bAirborne && FMath::Abs(MotionOffset) < 0.05f && FMath::Abs(MotionVelocity) < 0.5f;
	if (bSettled)
	{
		MotionOffset = 0.f;
		MotionVelocity = 0.f;
	}

	if (Panel_HUDMotion)
	{
		Panel_HUDMotion->SetRenderTranslation(FVector2D(0.f, MotionOffset));
	}

	if (!bSettled)
	{
		return EActiveTimerReturnType::Continue;
	}

	MotionTimerHandle.Reset();
	return EActiveTimerReturnType::Stop;
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
