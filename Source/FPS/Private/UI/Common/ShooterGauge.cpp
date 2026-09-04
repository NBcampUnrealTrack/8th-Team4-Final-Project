// ShooterGauge.cpp
#include "UI/Common/ShooterGauge.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"

void UShooterGauge::SetProgress(float InProgress)
{
	TargetProgress = FMath::Clamp(InProgress, 0.f, 1.f);
}

void UShooterGauge::SetProgressImmediate(float InProgress)
{
	SetProgress(InProgress);

	DisplayProgress = TargetProgress;

	// 이미 도달한 상태라 Tick 의 조기 return 에 걸린다. 
	// 플래그를 되돌려 한 번 더 넘기게 한다
	bParameterApplied = false;
}

void UShooterGauge::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	// 목표에 도달했고 한 번은 넘겼으면 셰이더를 다시 갱신하지 않는다
	if (bParameterApplied == true && FMath::IsNearlyEqual(DisplayProgress, TargetProgress))
	{
		return;
	}
	
	if (InterpSpeed > 0.f)
	{
		DisplayProgress = FMath::FInterpTo(DisplayProgress, TargetProgress, InDeltaTime, InterpSpeed);
	}
	else
	{
		DisplayProgress = TargetProgress;
	}
	
	if (IsValid(GetBarMID()))
	{
		if (UMaterialInstanceDynamic* MID = GetBarMID())
		{
			// Progress라는 ScalarParameter 노드에 적용
			MID->SetScalarParameterValue(TEXT("Progress"), DisplayProgress);
			bParameterApplied = true;
		}
	}
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
		
		if (CachedMID && BarTexture)
		{
			CachedMID->SetTextureParameterValue(TEXT("BarTexture"), BarTexture);
		}
	}
	return CachedMID;
}
