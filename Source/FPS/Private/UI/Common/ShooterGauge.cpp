// ShooterGauge.cpp
#include "UI/Common/ShooterGauge.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Widgets/SWidget.h"

void UShooterGauge::SetProgress(float InProgress)
{
	TargetProgress = FMath::Clamp(InProgress, 0.f, 1.f);

	if (InterpSpeed <= 0.f)
	{
		SetProgressImmediate(TargetProgress);
		return;
	}

	EnsureUpdateScheduled();
}

void UShooterGauge::SetProgressImmediate(float InProgress)
{
	TargetProgress = FMath::Clamp(InProgress, 0.f, 1.f);
	DisplayProgress = TargetProgress;

	ApplyDisplayProgress();
}

void UShooterGauge::NativeConstruct()
{
	Super::NativeConstruct();

	// 생성 전에 온 값도 여기서 반영
	ApplyDisplayProgress();

	if (!FMath::IsNearlyEqual(DisplayProgress, TargetProgress))
	{
		EnsureUpdateScheduled();
	}
}

void UShooterGauge::NativeDestruct()
{
	StopUpdates();

	Super::NativeDestruct();
}

void UShooterGauge::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	
	if (Image_Bg)
	{
		if (BgTexture)
		{
			Image_Bg->SetBrushFromTexture(BgTexture);
			Image_Bg->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_Bg->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

UMaterialInstanceDynamic* UShooterGauge::GetBarMID()
{
	if (IsDesignTime())
	{
		return nullptr;
	}
	
	if (!CachedMID && Image_Bar)
	{
		CachedMID = Image_Bar->GetDynamicMaterial();

		// 틱 재시도가 없어졌으니 설정 오류는 여기서 드러낸다
		ensureMsgf(CachedMID, TEXT("%s: Image_Bar 브러시가 머티리얼이 아닙니다"), *GetName());
		
		if (CachedMID && BarTexture)
		{
			CachedMID->SetTextureParameterValue(TEXT("BarTexture"), BarTexture);
		}
	}
	return CachedMID;
}

void UShooterGauge::EnsureUpdateScheduled()
{
	if (IsDesignTime() || UpdateTimerHandle.IsValid())
	{
		return;
	}

	// Slate 위젯이 아직 없으면 NativeConstruct 가 다시 부른다
	const TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
	if (!CachedWidget.IsValid())
	{
		return;
	}

	UpdateTimerHandle = CachedWidget->RegisterActiveTimer(
		0.f,
		FWidgetActiveTimerDelegate::CreateUObject(this, &ThisClass::HandleUpdateTimer));
}

void UShooterGauge::StopUpdates()
{
	const TSharedPtr<FActiveTimerHandle> Handle = UpdateTimerHandle.Pin();
	const TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
	if (Handle.IsValid() && CachedWidget.IsValid())
	{
		CachedWidget->UnRegisterActiveTimer(Handle.ToSharedRef());
	}

	UpdateTimerHandle.Reset();
}

EActiveTimerReturnType UShooterGauge::HandleUpdateTimer(double InCurrentTime, float InDeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ShooterGauge_UpdateTimer);

	DisplayProgress = FMath::FInterpTo(DisplayProgress, TargetProgress, InDeltaTime, InterpSpeed);
	ApplyDisplayProgress();

	if (!FMath::IsNearlyEqual(DisplayProgress, TargetProgress))
	{
		return EActiveTimerReturnType::Continue;
	}

	UpdateTimerHandle.Reset();
	return EActiveTimerReturnType::Stop;
}

void UShooterGauge::ApplyDisplayProgress()
{
	if (UMaterialInstanceDynamic* MID = GetBarMID())
	{
		// Progress라는 ScalarParameter 노드에 적용
		MID->SetScalarParameterValue(TEXT("Progress"), DisplayProgress);
	}
}
