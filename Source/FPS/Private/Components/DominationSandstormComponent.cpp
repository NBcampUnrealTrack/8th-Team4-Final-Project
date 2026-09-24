#include "Components/DominationSandstormComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PostProcessComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/Settings/DominationSandstormSettings.h"
#include "GameMode/State/DominationGameState.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationSandstormVisuals, Log, All);

UDominationSandstormComponent::UDominationSandstormComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(false);
}

void UDominationSandstormComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeForWorld();
	if (LocalController.IsValid())
	{
		FWorldDelegates::LevelAddedToWorld.AddUObject(this, &ThisClass::HandleLevelChanged);
		FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &ThisClass::HandleLevelChanged);
	}
}

void UDominationSandstormComponent::InitializeForWorld()
{
	ResetWorldBinding();
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (GetNetMode() == NM_DedicatedServer || !Controller || !Controller->IsLocalController())
	{
		return;
	}

	LocalController = Controller;
	// GameState가 준비되면 타이머를 끄고 복제 이벤트만 받는다.
	GetWorld()->GetTimerManager().SetTimer(BindingTimer, this, &ThisClass::BindGameState, 0.1f, true);
	BindGameState();
}

void UDominationSandstormComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FWorldDelegates::LevelAddedToWorld.RemoveAll(this);
	FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);
	ResetWorldBinding();
	Super::EndPlay(EndPlayReason);
}

void UDominationSandstormComponent::ResetWorldBinding()
{
	GetWorld()->GetTimerManager().ClearTimer(BindingTimer);
	if (BoundGameState.IsValid())
	{
		BoundGameState->OnSandstormStateChanged.RemoveAll(this);
		BoundGameState->OnDominationFlowStateChanged.RemoveAll(this);
	}
	if (AssetLoadHandle.IsValid())
	{
		AssetLoadHandle->CancelHandle();
		AssetLoadHandle.Reset();
	}
	StopPresentation();
	ReleaseVisualComponents();
	BoundGameState.Reset();
	LocalController.Reset();
	Settings = nullptr;
	LoadedNiagara = nullptr;
	CurrentState = FDominationSandstormState();
	LocalPreviewIntensity = -1.0f;
}

void UDominationSandstormComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdatePresentation();
}

void UDominationSandstormComponent::SetPreviewIntensity(const float Intensity)
{
#if !UE_BUILD_SHIPPING
	LocalPreviewIntensity = FMath::Clamp(Intensity, -1.0f, 1.0f);
	if (LocalController.IsValid())
	{
		UpdatePresentation();
	}
#endif
}

void UDominationSandstormComponent::BindGameState()
{
	ADominationGameState* GameState = GetWorld()->GetGameState<ADominationGameState>();
	if (!GameState)
	{
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(BindingTimer);
	BoundGameState = GameState;
	GameState->OnSandstormStateChanged.AddUniqueDynamic(this, &ThisClass::HandleSandstormStateChanged);
	GameState->OnDominationFlowStateChanged.AddUniqueDynamic(this, &ThisClass::HandleMatchFlowChanged);
	HandleSandstormStateChanged(GameState->GetSandstormState());
}

void UDominationSandstormComponent::HandleLevelChanged(ULevel* Level, UWorld* World)
{
	if (World != GetWorld() || !bPresentationActive)
	{
		return;
	}
	if (!Level)
	{
		StopPresentation();
		return;
	}
	DiscoverFog();
}

void UDominationSandstormComponent::HandleSandstormStateChanged(const FDominationSandstormState& State)
{
	CurrentState = State;
	// 설정 OnRep도 같은 이벤트를 보내므로 도착 순서에 의존하지 않는다.
	SetSettings(BoundGameState->GetSandstormSettings());
	UpdatePresentation();
}

void UDominationSandstormComponent::HandleMatchFlowChanged()
{
	HandleSandstormStateChanged(BoundGameState->GetSandstormState());
}

void UDominationSandstormComponent::SetSettings(UDominationSandstormSettings* NewSettings)
{
	if (Settings == NewSettings)
	{
		return;
	}
	if (AssetLoadHandle.IsValid())
	{
		AssetLoadHandle->CancelHandle();
		AssetLoadHandle.Reset();
	}
	StopPresentation();
	ReleaseVisualComponents();
	Settings = NewSettings;
	LoadedNiagara = nullptr;
	if (!Settings || Settings->NiagaraSystem.IsNull())
	{
		return;
	}
	if (Settings->NiagaraSystem.IsValid())
	{
		OnNiagaraLoaded();
		return;
	}
	AssetLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Settings->NiagaraSystem.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &ThisClass::OnNiagaraLoaded));
}

void UDominationSandstormComponent::OnNiagaraLoaded()
{
	LoadedNiagara = Settings->NiagaraSystem.Get();
	if (!LoadedNiagara)
	{
		UE_LOG(LogDominationSandstormVisuals, Warning,
			TEXT("Sandstorm Niagara load failed: %s"),
			*Settings->NiagaraSystem.ToString());
	}
	UpdatePresentation();
}

void UDominationSandstormComponent::EnsureVisualComponents()
{
	if (!LocalVisualActor)
	{
		FActorSpawnParameters Spawn;
		Spawn.ObjectFlags |= RF_Transient;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		LocalVisualActor = GetWorld()->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
		if (!LocalVisualActor) { return; }
		LocalVisualActor->SetReplicates(false);
		LocalVisualActor->SetActorHiddenInGame(false);
		LocalVisualActor->SetActorEnableCollision(false);
	}
	bool bCreatedComponent = false;
	if (!StormPostProcess)
	{
		bCreatedComponent = true;
		StormPostProcess = NewObject<UPostProcessComponent>(LocalVisualActor, NAME_None, RF_Transient);
		StormPostProcess->bUnbound = true;
		StormPostProcess->Priority = 100.0f;
		StormPostProcess->BlendWeight = 0.0f;
		StormPostProcess->RegisterComponent();
	}
	if (LoadedNiagara && !SandParticles)
	{
		bCreatedComponent = true;
		SandParticles = NewObject<UNiagaraComponent>(LocalVisualActor, NAME_None, RF_Transient);
		SandParticles->SetAutoActivate(false);
		SandParticles->SetAutoDestroy(false);
		SandParticles->SetAbsolute(true, true, true);
		SandParticles->SetAsset(LoadedNiagara);
		SandParticles->SetWorldLocationAndRotation(GetCameraLocation(), FRotator::ZeroRotator);
		SandParticles->SetForceLocalPlayerEffect(true);
		// 카메라 회전에 따른 significance/visibility cull로 재시작하지 않는다.
		SandParticles->SetAllowScalability(false);
		SandParticles->RegisterComponent();
		SandParticles->AddTickPrerequisiteComponent(this);
	}
	if (bCreatedComponent) { ConfigureVisualComponents(); }
}

void UDominationSandstormComponent::ConfigureVisualComponents()
{
	LastAppliedIntensity = -1.0f;
	if (SandParticles)
	{
		const FVector WindDirection = Settings->GetNormalizedWindDirection();
		SandParticles->SetVariableVec3(TEXT("User.WindDirection"), WindDirection);
		const FVector WindVelocity = WindDirection * FMath::Max(0.0f, Settings->WindSpeed);
		SandParticles->SetVariableVec3(TEXT("User.NearWindVelocity"), WindVelocity);
		SandParticles->SetVariableVec3(TEXT("User.MidWindVelocity"), WindVelocity * FMath::Max(0.0f, Settings->MidWindSpeedMultiplier));
		SandParticles->SetVariableVec3(TEXT("User.DustWindVelocity"), WindVelocity * FMath::Max(0.0f, Settings->DustWindSpeedMultiplier));
		SandParticles->SetVariableFloat(TEXT("User.WindSpeed"), FMath::Max(0.0f, Settings->WindSpeed));
		SandParticles->SetVariableFloat(TEXT("User.TurbulenceStrength"), FMath::Max(0.0f, Settings->TurbulenceStrength));
		SandParticles->SetVariableFloat(TEXT("User.NearSpawnMultiplier"), FMath::Max(0.0f, Settings->NearSpawnMultiplier));
		SandParticles->SetVariableFloat(TEXT("User.MidSpawnMultiplier"), FMath::Max(0.0f, Settings->MidSpawnMultiplier));
		SandParticles->SetVariableFloat(TEXT("User.DustSpawnMultiplier"), FMath::Max(0.0f, Settings->DustSpawnMultiplier));
		SandParticles->SetVariableFloat(TEXT("User.NearParticleRadius"), FMath::Max(100.0f, Settings->NearParticleRadius));
		SandParticles->SetVariableFloat(TEXT("User.MidParticleRadius"), FMath::Max(100.0f, Settings->MidParticleRadius));
		SandParticles->SetVariableFloat(TEXT("User.DustParticleRadius"), FMath::Max(100.0f, Settings->DustParticleRadius));
		const float Extent = FMath::Max(100.0f, FMath::Max3(Settings->NearParticleRadius, Settings->MidParticleRadius, Settings->DustParticleRadius))
			+ FMath::Max(1000.0f, Settings->ParticleBoundsPadding);
		SandParticles->SetSystemFixedBounds(FBox(-FVector(Extent), FVector(Extent)));
	}
	if (StormPostProcess)
	{
		FPostProcessSettings& Post = StormPostProcess->Settings;
		Post.bOverride_ColorSaturation = true;
		const float Saturation = 1.0f - FMath::Clamp(Settings->Desaturation, 0.0f, 1.0f);
		Post.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.0f);
		Post.bOverride_ColorContrast = true;
		Post.ColorContrast = FVector4(Settings->ContrastAdjustment, Settings->ContrastAdjustment, Settings->ContrastAdjustment, 1.0f);
		Post.bOverride_SceneColorTint = true;
		const float MaxChannel = FMath::Max3(Settings->SandTint.R, Settings->SandTint.G, Settings->SandTint.B);
		FLinearColor NormalizedTint = MaxChannel > UE_SMALL_NUMBER ? Settings->SandTint / MaxChannel : FLinearColor::White;
		NormalizedTint.A = 1.0f;
		Post.SceneColorTint = FMath::Lerp(FLinearColor::White, NormalizedTint, FMath::Clamp(Settings->PostProcessTintStrength, 0.0f, 1.0f));
	}
}

FVector UDominationSandstormComponent::GetCameraLocation() const
{
	const APlayerController* Controller = LocalController.Get();
	if (Controller && Controller->PlayerCameraManager)
	{
		return Controller->PlayerCameraManager->GetCameraLocation();
	}
	return Controller ? Controller->GetFocalLocation() : FVector::ZeroVector;
}

void UDominationSandstormComponent::UpdatePresentation()
{
	if (!LocalController.IsValid() || !Settings || !BoundGameState.IsValid()
		|| BoundGameState->GetDominationPhase() == EDominationMatchPhase::MatchEnded)
	{
		StopPresentation();
		return;
	}
	const double ServerTime = BoundGameState->GetServerWorldTimeSeconds();
	StormIntensity = CurrentState.GetIntensity(ServerTime);
#if !UE_BUILD_SHIPPING
	const float Preview = LocalPreviewIntensity >= 0.0f ? LocalPreviewIntensity : Settings->PreviewIntensity;
	if (Preview >= 0.0f)
	{
		StormIntensity = FMath::Clamp(Preview, 0.0f, 1.0f);
	}
#endif
	if (StormIntensity > UE_SMALL_NUMBER)
	{
		EnsureVisualComponents();
		if (!bPresentationActive)
		{
			bPresentationActive = true;
			DiscoverFog();
		}
		UpdateCameraLocation();
		if (LastAppliedIntensity != StormIntensity)
		{
			ApplyIntensity();
		}
		if (SandParticles && !SandParticles->IsActive()) { SandParticles->Activate(true); }
	}
	else if (bPresentationActive)
	{
		bPresentationActive = false;
		RestoreFog();
		if (StormPostProcess) { StormPostProcess->BlendWeight = 0.0f; }
		if (SandParticles)
		{
			SandParticles->SetVariableFloat(TEXT("User.StormIntensity"), 0.0f);
			SandParticles->DeactivateImmediate();
		}
	}
	const bool bInterpolating = (CurrentState.Phase == EDominationSandstormPhase::FadeIn
		|| CurrentState.Phase == EDominationSandstormPhase::FadeOut) && CurrentState.GetTimeRemaining(ServerTime) > 0.0f;
	SetComponentTickEnabled(bPresentationActive || bInterpolating);
}

void UDominationSandstormComponent::UpdateCameraLocation()
{
	const FVector CameraLocation = GetCameraLocation();
	if (LocalFog) { LocalFog->SetActorLocation(CameraLocation); }
	if (SandParticles)
	{
		// 기존 파티클은 월드 공간에 남기고 생성 위치만 카메라를 따라간다.
		SandParticles->SetWorldLocationAndRotation(CameraLocation, FRotator::ZeroRotator);
	}
}

void UDominationSandstormComponent::ApplyIntensity()
{
	ApplyFog(StormIntensity);
	if (StormPostProcess) { StormPostProcess->BlendWeight = StormIntensity; }
	if (SandParticles)
	{
		SandParticles->SetVariableFloat(TEXT("User.StormIntensity"), StormIntensity);
		SandParticles->SetVariableFloat(TEXT("User.NearParticleSpawnRate"),
			FMath::Max(0.0f, Settings->NearParticleSpawnRate * Settings->NearSpawnMultiplier) * StormIntensity);
		SandParticles->SetVariableFloat(TEXT("User.MidParticleSpawnRate"),
			FMath::Max(0.0f, Settings->MidParticleSpawnRate * Settings->MidSpawnMultiplier) * StormIntensity);
		SandParticles->SetVariableFloat(TEXT("User.DustParticleSpawnRate"),
			FMath::Max(0.0f, Settings->DustParticleSpawnRate * Settings->DustSpawnMultiplier) * StormIntensity);
	}
	LastAppliedIntensity = StormIntensity;
}

void UDominationSandstormComponent::CaptureFog(UExponentialHeightFogComponent* Fog)
{
	if (!IsValid(Fog) || FogSnapshots.ContainsByPredicate([Fog](const FDominationFogSnapshot& Snapshot) { return Snapshot.Component.Get() == Fog; }))
	{
		return;
	}
	FDominationFogSnapshot& Snapshot = FogSnapshots.AddDefaulted_GetRef();
	LastAppliedIntensity = -1.0f;
	Snapshot.Component = Fog;
	Snapshot.Density = Fog->FogDensity;
	Snapshot.StartDistance = Fog->StartDistance;
	Snapshot.HeightFalloff = Fog->FogHeightFalloff;
	Snapshot.MaxOpacity = Fog->FogMaxOpacity;
	Snapshot.CutoffDistance = Fog->FogCutoffDistance;
	Snapshot.Color = Fog->FogInscatteringLuminance;
	Snapshot.CubemapTint = Fog->InscatteringTextureTint;
	Snapshot.DirectionalColor = Fog->DirectionalInscatteringLuminance;
	Snapshot.SecondFog = Fog->SecondFogData;
	Snapshot.bVolumetricFog = Fog->bEnableVolumetricFog;
	Snapshot.VolumetricEmissive = Fog->VolumetricFogEmissive;
}

void UDominationSandstormComponent::DiscoverFog()
{
	bool bHasMapFog = false;
	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
	{
		if (*It == LocalFog || It->IsHidden()) { continue; }
		UExponentialHeightFogComponent* Fog = It->GetComponent();
		if (IsValid(Fog) && Fog->IsRegistered() && Fog->IsVisible())
		{
			bHasMapFog = true;
			CaptureFog(Fog);
		}
	}
	if (bHasMapFog && LocalFog)
	{
		UExponentialHeightFogComponent* OldFog = LocalFog->GetComponent();
		FogSnapshots.RemoveAll([OldFog](const FDominationFogSnapshot& Snapshot) { return Snapshot.Component.Get() == OldFog; });
		LocalFog->Destroy();
		LocalFog = nullptr;
	}
	if (!bHasMapFog && !LocalFog)
	{
		FActorSpawnParameters Spawn;
		Spawn.Owner = LocalController.Get();
		Spawn.ObjectFlags |= RF_Transient;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		LocalFog = GetWorld()->SpawnActor<AExponentialHeightFog>(GetCameraLocation(), FRotator::ZeroRotator, Spawn);
		if (LocalFog)
		{
			LocalFog->SetReplicates(false);
			UExponentialHeightFogComponent* Fog = LocalFog->GetComponent();
			Fog->SetMobility(EComponentMobility::Movable);
			Fog->SetFogDensity(0.0f);
			CaptureFog(Fog);
		}
	}
}

void UDominationSandstormComponent::ApplyFog(const float Intensity)
{
	for (const FDominationFogSnapshot& Snapshot : FogSnapshots)
	{
		UExponentialHeightFogComponent* Fog = Snapshot.Component.Get();
		if (!Fog) { continue; }
		const float BaseDensity = Settings->bUseMapFogBaseline ? Snapshot.Density : Settings->NormalFogDensity;
		const float BaseStart = Settings->bUseMapFogBaseline ? Snapshot.StartDistance : Settings->NormalFogStartDistance;
		Fog->SetFogDensity(FMath::Lerp(BaseDensity, FMath::Max(0.0f, Settings->PeakFogDensity), Intensity));
		Fog->SetStartDistance(FMath::Lerp(BaseStart, Settings->PeakFogStartDistance, Intensity));
		Fog->SetFogHeightFalloff(FMath::Lerp(Snapshot.HeightFalloff, Settings->PeakFogHeightFalloff, Intensity));
		Fog->SetFogMaxOpacity(FMath::Lerp(Snapshot.MaxOpacity, Settings->PeakFogMaxOpacity, Intensity));
		Fog->SetFogInscatteringColor(FMath::Lerp(Snapshot.Color, Settings->SandTint, Intensity));
		Fog->SetDirectionalInscatteringColor(FMath::Lerp(Snapshot.DirectionalColor, Settings->SandTint, Intensity));
		// 기존 큐브맵을 갑자기 제거하지 않고 색을 보간한다.
		Fog->SetInscatteringTextureTint(FMath::Lerp(Snapshot.CubemapTint, Snapshot.CubemapTint * Settings->SandTint, Intensity));
		Fog->SetFogCutoffDistance(0.0f);
		FExponentialHeightFogData SecondFog = Snapshot.SecondFog;
		SecondFog.FogDensity *= 1.0f - Intensity;
		Fog->SetSecondFogData(SecondFog);
		Fog->SetVolumetricFog(Settings->bEnableVolumetricFog || Snapshot.bVolumetricFog);
		// r.SupportExpFogMatchesVolumetricFog도 동일한 색을 사용하도록 한다.
		Fog->SetVolumetricFogEmissive(FMath::Lerp(Snapshot.VolumetricEmissive, Settings->SandTint, Intensity));
	}
}

void UDominationSandstormComponent::RestoreFog()
{
	LastAppliedIntensity = -1.0f;
	for (const FDominationFogSnapshot& Snapshot : FogSnapshots)
	{
		UExponentialHeightFogComponent* Fog = Snapshot.Component.Get();
		if (!Fog) { continue; }
		Fog->SetFogDensity(Snapshot.Density);
		Fog->SetStartDistance(Snapshot.StartDistance);
		Fog->SetFogHeightFalloff(Snapshot.HeightFalloff);
		Fog->SetFogMaxOpacity(Snapshot.MaxOpacity);
		Fog->SetFogInscatteringColor(Snapshot.Color);
		Fog->SetDirectionalInscatteringColor(Snapshot.DirectionalColor);
		Fog->SetFogCutoffDistance(Snapshot.CutoffDistance);
		Fog->SetInscatteringTextureTint(Snapshot.CubemapTint);
		Fog->SetSecondFogData(Snapshot.SecondFog);
		Fog->SetVolumetricFog(Snapshot.bVolumetricFog);
		Fog->SetVolumetricFogEmissive(Snapshot.VolumetricEmissive);
	}
	FogSnapshots.Reset();
	if (LocalFog)
	{
		LocalFog->Destroy();
		LocalFog = nullptr;
	}
}

void UDominationSandstormComponent::StopPresentation()
{
	StormIntensity = 0.0f;
	bPresentationActive = false;
	RestoreFog();
	if (SandParticles) { SandParticles->DeactivateImmediate(); }
	if (StormPostProcess) { StormPostProcess->BlendWeight = 0.0f; }
	SetComponentTickEnabled(false);
}

void UDominationSandstormComponent::ReleaseVisualComponents()
{
	if (SandParticles) { SandParticles->DestroyComponent(); SandParticles = nullptr; }
	if (StormPostProcess) { StormPostProcess->DestroyComponent(); StormPostProcess = nullptr; }
	if (LocalVisualActor) { LocalVisualActor->Destroy(); LocalVisualActor = nullptr; }
}
