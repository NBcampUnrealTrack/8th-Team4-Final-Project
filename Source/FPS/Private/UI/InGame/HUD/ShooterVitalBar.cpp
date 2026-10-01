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

	const FName BurstCellStep(TEXT("BurstCellStep"));
	const FName BurstChunkSeconds(TEXT("BurstChunkSeconds"));
	const FName BurstQuadScale(TEXT("BurstQuadScale"));

	const FName BurstScalars[] = { BurstCellStep, BurstChunkSeconds, BurstQuadScale };

	const FName Bursts[] =
	{
		FName(TEXT("Burst0")), FName(TEXT("Burst1")), FName(TEXT("Burst2")),
		FName(TEXT("Burst3")), FName(TEXT("Burst4")), FName(TEXT("Burst5"))
	};
	static_assert(static_cast<int32>(UE_ARRAY_COUNT(Bursts)) == FShooterVitalBarBurst::MaxInstances, "Burst vector count must match the ring size");

	// 바 머티리얼 값을 그대로 쓰는 파라미터
	const FName SharedWithBar[] =
	{
		FName(TEXT("AxisSlope")), FName(TEXT("AxisStart")), FName(TEXT("AxisEnd")),
		FName(TEXT("DividerWidthPx")), FName(TEXT("EdgeSoftnessPx"))
	};

#if !UE_BUILD_SHIPPING
	bool HasParameter(const TArray<FMaterialParameterInfo>& Infos, const FName& Name)
	{
		return Infos.ContainsByPredicate([&Name](const FMaterialParameterInfo& Info)
		{
			return Info.Name == Name;
		});
	}

	// 이름이 틀리면 Set*ParameterValue 가 조용히 무시한다
	void ReportMissing(const UMaterialInstanceDynamic& MID, const UObject& Owner, TConstArrayView<FName> Scalars, TConstArrayView<FName> Vectors)
	{
		TArray<FMaterialParameterInfo> ScalarInfos;
		TArray<FMaterialParameterInfo> VectorInfos;
		TArray<FGuid> Ids;
		MID.GetAllScalarParameterInfo(ScalarInfos, Ids);
		MID.GetAllVectorParameterInfo(VectorInfos, Ids);

		for (const FName& Name : Scalars)
		{
			if (!HasParameter(ScalarInfos, Name))
			{
				UE_LOG(LogShooterVitalBar, Warning, TEXT("[VitalBar] %s: %s 에 '%s' 파라미터가 없습니다"),
					*Owner.GetName(), *MID.GetName(), *Name.ToString());
			}
		}

		for (const FName& Name : Vectors)
		{
			if (!HasParameter(VectorInfos, Name))
			{
				UE_LOG(LogShooterVitalBar, Warning, TEXT("[VitalBar] %s: %s 에 '%s' 파라미터가 없습니다"),
					*Owner.GetName(), *MID.GetName(), *Name.ToString());
			}
		}
	}
#endif
}

void UShooterVitalBar::SetVitals(const FShooterVitalValues& InValues)
{
	const double Now = GetSlateTime();

	// 다른 프레임 값이면 앞 값을 그 시각에 먼저 반영
	if (bHasPending && PendingTime != Now)
	{
		CommitPending();
	}

	PendingValues = InValues;
	PendingTime = Now;
	bHasPending = true;

	EnsureUpdateScheduled();
}

void UShooterVitalBar::SetVitalsImmediate(const FShooterVitalValues& InValues)
{
	bHasPending = false;

	Model.QueueTarget(InValues);
	Model.SnapToTarget();
	ClockTime = GetSlateTime();

	Burst.Reset(Model.GetHealthTrack().Real, Model.GetShieldTrack().Real);
	SetBurstVisible(false);

	StopUpdates();
	ApplyLayout(Model.GetLayout(Settings));
}

void UShooterVitalBar::NativeConstruct()
{
	Super::NativeConstruct();

	// WBP 에 저장된 가시성과 상관없이 숨김에서 시작
	bBurstVisible = true;
	SetBurstVisible(false);

	// 조각 파라미터 이름을 시작 때 검사
	GetBurstMID();

	// 생성 전이나 떨어져 있던 동안 온 값도 여기서 처리
	EnsureUpdateScheduled();
}

void UShooterVitalBar::NativeDestruct()
{
	StopUpdates();

	// 대기 값과 모델, 시계는 남긴다
	Burst.Reset(Model.GetHealthTrack().Real, Model.GetShieldTrack().Real);
	SetBurstVisible(false);

	Super::NativeDestruct();
}

void UShooterVitalBar::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// 조각 쿼드만 세로로 키운다
	if (Image_DamageBurst)
	{
		Image_DamageBurst->SetRenderScale(FVector2D(1.f, BurstSettings.QuadHeightScale));
	}

	if (!IsDesignTime())
	{
		return;
	}

	// 디자이너가 브러시를 다시 만들 수 있다
	CachedMID = nullptr;
	CachedBurstMID = nullptr;

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
	CommitPending();

	UE_LOG(LogShooterVitalBar, VeryVerbose, TEXT("[VitalBar] %s: 갱신 Dt=%.3f"), *GetName(), InCurrentTime - ClockTime);

	// 숨어 있던 시간까지 한 번에 소화
	const FShooterVitalBarTickResult Result = AdvanceTo(InCurrentTime);

	const FShooterVitalBarLayout Layout = Model.GetLayout(Settings);
	if (Layout != LastAppliedLayout)
	{
		ApplyLayout(Layout);
	}

	ApplyBurst();

	if (Result.bNeedsMoreUpdates || Burst.IsActive())
	{
		return EActiveTimerReturnType::Continue;
	}

	UE_LOG(LogShooterVitalBar, Verbose, TEXT("[VitalBar] %s: 갱신 멈춤"), *GetName());
	UpdateTimerHandle.Reset();
	return EActiveTimerReturnType::Stop;
}

void UShooterVitalBar::CommitPending()
{
	if (!bHasPending)
	{
		return;
	}
	bHasPending = false;

	// 그 피격 시각까지 먼저 진행
	AdvanceTo(PendingTime);

	const float HealthBefore = Model.GetHealthTrack().Real;
	const float ShieldBefore = Model.GetShieldTrack().Real;

	Model.QueueTarget(PendingValues);
	Model.Tick(0.f, Settings);

	// 조각 이미지가 없으면 조각도 만들지 않는다
	if (!Image_DamageBurst)
	{
		return;
	}

	const FShooterVitalBarLayout Layout = Model.GetLayout(Settings);

	FShooterVitalBarBurstInput In;
	In.HealthBefore = HealthBefore;
	In.HealthAfter = Model.GetHealthTrack().Real;
	In.MaxHealth = Model.GetMaxHealth();
	In.ShieldBefore = ShieldBefore;
	In.ShieldAfter = Model.GetShieldTrack().Real;
	In.MaxShield = Model.GetMaxShield();
	In.DisplayD = Layout.DisplayD;
	In.ShieldStart = Layout.EdgeHP * Layout.DisplayD;
	Burst.Emit(In, BurstSettings);
}

FShooterVitalBarTickResult UShooterVitalBar::AdvanceTo(const double Time)
{
	const float DeltaSeconds = static_cast<float>(FMath::Max(0.0, Time - ClockTime));
	ClockTime = FMath::Max(ClockTime, Time);

	Burst.Tick(DeltaSeconds);
	return Model.Tick(DeltaSeconds, Settings);
}

void UShooterVitalBar::ApplyLayout(const FShooterVitalBarLayout& Layout)
{
	LastAppliedLayout = Layout;

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

void UShooterVitalBar::ApplyBurst()
{
	if (!Image_DamageBurst)
	{
		return;
	}

	const bool bActive = Burst.IsActive();
	SetBurstVisible(bActive);
	if (!bActive)
	{
		return;
	}

	UMaterialInstanceDynamic* MID = GetBurstMID();
	if (!MID)
	{
		return;
	}

	// 칸이 값 기준이라 D 가 바뀌면 폭도 바뀐다
	const float CellStep = (LastAppliedLayout.DisplayD > 0.f) ? BurstSettings.CellValue / LastAppliedLayout.DisplayD : 0.f;
	MID->SetScalarParameterValue(ShooterVitalBarParams::BurstCellStep, CellStep);

	for (int32 Index = 0; Index < FShooterVitalBarBurst::MaxInstances; ++Index)
	{
		const FShooterVitalBurstInstance& Instance = Burst.GetInstance(Index);
		const FLinearColor Value = Instance.bActive
			? FLinearColor(Instance.FirstCell, Instance.LastCell, Instance.Age, Instance.Stagger)
			: FLinearColor(0.f, -1.f, 0.f, 0.f);
		MID->SetVectorParameterValue(ShooterVitalBarParams::Bursts[Index], Value);
	}
}

void UShooterVitalBar::SetBurstVisible(const bool bVisible)
{
	if (!Image_DamageBurst || bBurstVisible == bVisible)
	{
		return;
	}

	bBurstVisible = bVisible;
	Image_DamageBurst->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	UE_LOG(LogShooterVitalBar, Verbose, TEXT("[VitalBar] %s: 조각 %s"), *GetName(), bVisible ? TEXT("표시") : TEXT("숨김"));
}

UMaterialInstanceDynamic* UShooterVitalBar::GetBarMID()
{
	if (!CachedMID && Image_Bar)
	{
		CachedMID = Image_Bar->GetDynamicMaterial();

#if !UE_BUILD_SHIPPING
		if (CachedMID && !IsDesignTime())
		{
			ShooterVitalBarParams::ReportMissing(*CachedMID, *this, ShooterVitalBarParams::All, {});
		}
#endif
	}

	return CachedMID;
}

UMaterialInstanceDynamic* UShooterVitalBar::GetBurstMID()
{
	if (!CachedBurstMID && Image_DamageBurst)
	{
		CachedBurstMID = Image_DamageBurst->GetDynamicMaterial();
		if (!CachedBurstMID)
		{
			return nullptr;
		}

		CachedBurstMID->SetScalarParameterValue(ShooterVitalBarParams::BurstChunkSeconds, BurstSettings.ChunkSeconds);
		CachedBurstMID->SetScalarParameterValue(ShooterVitalBarParams::BurstQuadScale, BurstSettings.QuadHeightScale);

		// 축과 선 폭은 바 머티리얼 값을 그대로
		if (UMaterialInstanceDynamic* BarMID = GetBarMID())
		{
			for (const FName& Name : ShooterVitalBarParams::SharedWithBar)
			{
				CachedBurstMID->SetScalarParameterValue(Name, BarMID->K2_GetScalarParameterValue(Name));
			}
		}

#if !UE_BUILD_SHIPPING
		if (!IsDesignTime())
		{
			ShooterVitalBarParams::ReportMissing(*CachedBurstMID, *this, ShooterVitalBarParams::BurstScalars, ShooterVitalBarParams::Bursts);
			ShooterVitalBarParams::ReportMissing(*CachedBurstMID, *this, ShooterVitalBarParams::SharedWithBar, {});
		}
#endif
	}

	return CachedBurstMID;
}

double UShooterVitalBar::GetSlateTime()
{
	return FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetCurrentTime() : 0.0;
}
