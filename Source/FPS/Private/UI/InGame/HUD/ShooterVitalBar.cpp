// ShooterVitalBar.cpp
#include "UI/InGame/HUD/ShooterVitalBar.h"

#include "Components/Image.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Widgets/SWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterVitalBar, Log, All);

namespace ShooterVitalBarParams
{
	const FName EdgeHPSolid(TEXT("Edge_HPSolid"));
	const FName EdgeHP(TEXT("Edge_HP"));
	const FName EdgeShieldSolid(TEXT("Edge_ShieldSolid"));
	const FName EdgeContent(TEXT("Edge_Content"));
	const FName EdgeMissingHP(TEXT("Edge_MissingHP"));
	const FName SegmentStep(TEXT("SegmentStep"));
	const FName HPChangeIsHeal(TEXT("HPChangeIsHeal"));
	const FName ShieldChangeIsHeal(TEXT("ShieldChangeIsHeal"));
	const FName HPFlash(TEXT("HPFlash"));
	const FName ShieldFlash(TEXT("ShieldFlash"));

	const FName All[] =
	{
		EdgeHPSolid, EdgeHP, EdgeShieldSolid, EdgeContent, EdgeMissingHP,
		SegmentStep, HPChangeIsHeal, ShieldChangeIsHeal, HPFlash, ShieldFlash
	};

#if !UE_BUILD_SHIPPING
	void ReportMissing(const UMaterialInstanceDynamic& MID, const UObject& Owner)
	{
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		MID.GetAllScalarParameterInfo(Infos, Ids);

		for (const FName& Name : All)
		{
			const bool bFound = Infos.ContainsByPredicate([&Name](const FMaterialParameterInfo& Info)
			{
				return Info.Name == Name;
			});

			if (!bFound)
			{
				UE_LOG(LogShooterVitalBar, Warning, TEXT("[VitalBar] %s: 머티리얼에 '%s' 파라미터가 없습니다"),
					*Owner.GetName(), *Name.ToString());
			}
		}
	}
#endif
}

void UShooterVitalBar::SetVitals(const FShooterVitalValues& InValues)
{
	Model.QueueTarget(InValues);
	EnsureUpdateScheduled();
}

void UShooterVitalBar::SetVitalsImmediate(const FShooterVitalValues& InValues)
{
	Model.QueueTarget(InValues);
	Model.SnapToTarget();
	StopUpdates();
	ApplyLayout(Model.GetLayout(Settings));
}

void UShooterVitalBar::NativeConstruct()
{
	Super::NativeConstruct();

	// 생성 전에 온 값도 여기서 처리
	EnsureUpdateScheduled();
}

void UShooterVitalBar::NativeDestruct()
{
	StopUpdates();

	Super::NativeDestruct();
}

void UShooterVitalBar::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (!IsDesignTime())
	{
		return;
	}

	// 디자이너가 브러시를 다시 만들 수 있다
	CachedMID = nullptr;

	FShooterVitalBarLayoutInput In;
	In.MaxHealth = DesignTimeVitals.MaxHealth;
	In.MaxShield = DesignTimeVitals.MaxShield;
	In.HealthReal = DesignTimeVitals.Health;
	In.HealthTrail = DesignTimeHealthTrail;
	In.ShieldReal = DesignTimeVitals.Shield;
	In.ShieldTrail = DesignTimeShieldTrail;
	In.SegmentValue = Settings.SegmentValue;
	ApplyLayout(FShooterVitalBarLayout::Compute(In));
}

void UShooterVitalBar::EnsureUpdateScheduled()
{
	if (IsDesignTime() || UpdateTimerHandle.IsValid() || !FSlateApplication::IsInitialized())
	{
		return;
	}

	// Slate 위젯이 아직 없으면 NativeConstruct 가 다시 부른다
	const TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
	if (!CachedWidget.IsValid())
	{
		return;
	}

	LastUpdateTime = FSlateApplication::Get().GetCurrentTime();
	UpdateTimerHandle = CachedWidget->RegisterActiveTimer(
		0.f,
		FWidgetActiveTimerDelegate::CreateUObject(this, &ThisClass::HandleUpdateTimer));

	UE_LOG(LogShooterVitalBar, Verbose, TEXT("[VitalBar] %s: 갱신 시작"), *GetName());
}

void UShooterVitalBar::StopUpdates()
{
	const TSharedPtr<FActiveTimerHandle> Handle = UpdateTimerHandle.Pin();
	const TSharedPtr<SWidget> CachedWidget = GetCachedWidget();
	if (Handle.IsValid() && CachedWidget.IsValid())
	{
		CachedWidget->UnRegisterActiveTimer(Handle.ToSharedRef());
	}

	UpdateTimerHandle.Reset();
}

EActiveTimerReturnType UShooterVitalBar::HandleUpdateTimer(const double InCurrentTime, float InDeltaTime)
{
	// 숨어 있던 시간까지 한 번에 소화
	const float DeltaSeconds = static_cast<float>(FMath::Max(0.0, InCurrentTime - LastUpdateTime));
	LastUpdateTime = InCurrentTime;

	const FShooterVitalBarTickResult Result = Model.Tick(DeltaSeconds, Settings);
	if (Result.bVisualChanged)
	{
		ApplyLayout(Model.GetLayout(Settings));
	}

	UE_LOG(LogShooterVitalBar, VeryVerbose, TEXT("[VitalBar] %s: 갱신 Dt=%.3f"), *GetName(), DeltaSeconds);

	if (Result.bNeedsMoreUpdates)
	{
		return EActiveTimerReturnType::Continue;
	}

	UE_LOG(LogShooterVitalBar, Verbose, TEXT("[VitalBar] %s: 갱신 멈춤"), *GetName());
	UpdateTimerHandle.Reset();
	return EActiveTimerReturnType::Stop;
}

void UShooterVitalBar::ApplyLayout(const FShooterVitalBarLayout& Layout)
{
	UMaterialInstanceDynamic* MID = GetBarMID();
	if (!MID)
	{
		return;
	}

	MID->SetScalarParameterValue(ShooterVitalBarParams::EdgeHPSolid, Layout.EdgeHPSolid);
	MID->SetScalarParameterValue(ShooterVitalBarParams::EdgeHP, Layout.EdgeHP);
	MID->SetScalarParameterValue(ShooterVitalBarParams::EdgeShieldSolid, Layout.EdgeShieldSolid);
	MID->SetScalarParameterValue(ShooterVitalBarParams::EdgeContent, Layout.EdgeContent);
	MID->SetScalarParameterValue(ShooterVitalBarParams::EdgeMissingHP, Layout.EdgeMissingHP);
	MID->SetScalarParameterValue(ShooterVitalBarParams::SegmentStep, Layout.SegmentStep);
	MID->SetScalarParameterValue(ShooterVitalBarParams::HPChangeIsHeal, Layout.bHealthChangeIsHeal ? 1.f : 0.f);
	MID->SetScalarParameterValue(ShooterVitalBarParams::ShieldChangeIsHeal, Layout.bShieldChangeIsHeal ? 1.f : 0.f);
	MID->SetScalarParameterValue(ShooterVitalBarParams::HPFlash, Layout.HealthFlash);
	MID->SetScalarParameterValue(ShooterVitalBarParams::ShieldFlash, Layout.ShieldFlash);
}

UMaterialInstanceDynamic* UShooterVitalBar::GetBarMID()
{
	if (!CachedMID && Image_Bar)
	{
		CachedMID = Image_Bar->GetDynamicMaterial();

#if !UE_BUILD_SHIPPING
		if (CachedMID && !IsDesignTime())
		{
			ShooterVitalBarParams::ReportMissing(*CachedMID, *this);
		}
#endif
	}

	return CachedMID;
}
