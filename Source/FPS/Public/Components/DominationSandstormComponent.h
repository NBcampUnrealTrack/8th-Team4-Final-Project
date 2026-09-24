#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/TimerHandle.h"
#include "GameMode/Types/DominationSandstormTypes.h"
#include "DominationSandstormComponent.generated.h"

class AActor;
class ADominationGameState;
class AExponentialHeightFog;
class APlayerController;
class UDominationSandstormSettings;
class UNiagaraComponent;
class UNiagaraSystem;
class ULevel;
class UPostProcessComponent;
struct FStreamableHandle;

USTRUCT()
struct FDominationFogSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UExponentialHeightFogComponent> Component;

	float Density = 0.0f;
	float StartDistance = 0.0f;
	float HeightFalloff = 0.0f;
	float MaxOpacity = 1.0f;
	float CutoffDistance = 0.0f;
	FLinearColor Color = FLinearColor::White;
	FLinearColor CubemapTint = FLinearColor::White;
	FLinearColor DirectionalColor = FLinearColor::White;
	FLinearColor VolumetricEmissive = FLinearColor::Black;
	FExponentialHeightFogData SecondFog;
	bool bVolumetricFog = false;
};

// 점령전 로컬 컨트롤러만 소유한다. 게임 규칙과 파티클은 복제하지 않는다.
UCLASS(ClassGroup = (Rendering))
class FPS_API UDominationSandstormComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDominationSandstormComponent();
	void InitializeForWorld();
	void ResetWorldBinding();

	UFUNCTION(BlueprintPure, Category = "Sandstorm")
	float GetStormIntensity() const { return StormIntensity; }

	UFUNCTION(BlueprintCallable, Category = "Sandstorm|Debug")
	void SetPreviewIntensity(float Intensity);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void HandleSandstormStateChanged(const FDominationSandstormState& State);

	UFUNCTION()
	void HandleMatchFlowChanged();

	void BindGameState();
	void HandleLevelChanged(ULevel* Level, UWorld* World);
	void SetSettings(UDominationSandstormSettings* NewSettings);
	void OnNiagaraLoaded();
	void EnsureVisualComponents();
	void ConfigureVisualComponents();
	void UpdatePresentation();
	void UpdateCameraLocation();
	void ApplyIntensity();
	void DiscoverFog();
	void CaptureFog(UExponentialHeightFogComponent* Fog);
	void ApplyFog(float Intensity);
	void RestoreFog();
	void StopPresentation();
	void ReleaseVisualComponents();
	FVector GetCameraLocation() const;

	UPROPERTY(Transient)
	TObjectPtr<UDominationSandstormSettings> Settings;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedNiagara;

	UPROPERTY(Transient)
	TObjectPtr<AActor> LocalVisualActor;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> SandParticles;

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> StormPostProcess;

	UPROPERTY(Transient)
	TObjectPtr<AExponentialHeightFog> LocalFog;

	UPROPERTY(Transient)
	TArray<FDominationFogSnapshot> FogSnapshots;

	TWeakObjectPtr<APlayerController> LocalController;
	TWeakObjectPtr<ADominationGameState> BoundGameState;
	TSharedPtr<FStreamableHandle> AssetLoadHandle;
	FTimerHandle BindingTimer;
	FDominationSandstormState CurrentState;
	float StormIntensity = 0.0f;
	float LastAppliedIntensity = -1.0f;
	float LocalPreviewIntensity = -1.0f;
	bool bPresentationActive = false;
};
