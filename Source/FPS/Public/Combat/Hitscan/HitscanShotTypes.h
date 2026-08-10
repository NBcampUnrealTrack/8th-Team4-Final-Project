#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "Combat/Hitscan/HitscanTypes.h"
#include "HitscanShotTypes.generated.h"

class AActor;
class UHitscanSurfaceDefinition;
class UPrimitiveComponent;

USTRUCT(BlueprintType)
struct FHitscanDamageTargetSettings
{
	GENERATED_BODY()

	// TODO(Character/GAS 담당): GAS의 Damageable 인터페이스가 dev에 반영되면
	// Bone/ActorTag 기반 판정 대신 해당 인터페이스를 최종 기준으로 연결한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Target")
	bool bAcceptBoneHits = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Target")
	FName DamageableActorTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Target")
	bool bRequireDamageableActorTag = false;
};

USTRUCT(BlueprintType)
struct FHitscanTraceSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace")
	bool bTraceComplex = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace", meta = (ClampMin = "0.0"))
	float MaximumRangeCentimeters = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace", meta = (ClampMin = "0.1"))
	float SurfaceExitSearchDistanceCentimeters = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace", meta = (ClampMin = "0.01"))
	float TraceAdvanceEpsilonCentimeters = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace", meta = (ClampMin = "1", ClampMax = "128"))
	int32 MaximumTraceIterations = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace", meta = (ClampMin = "1", ClampMax = "128"))
	int32 MaximumPelletCount = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace", meta = (ClampMin = "0.1"))
	float FallbackSurfaceThicknessCentimeters = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace")
	bool bUsePenetrableSurfaceComponentOverrides = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace")
	FHitscanSurfaceRule DefaultSurfaceRule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace")
	bool bAllowZeroCostSurfaceRule = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace")
	FHitscanDamageTargetSettings DamageTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Debug")
	bool bDrawDebugTrace = false;
};

USTRUCT(BlueprintType)
struct FHitscanShotRequest
{
	GENERATED_BODY()

	// TODO(A 담당): 서버에서 발사 가능 여부와 탄약/RPM 검증이 끝난 뒤 한 번만 전달한다.
	// 기존 AWeaponBase Trace와 동시에 실행하면 판정 및 대미지가 중복될 수 있다.
	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Shot")
	TObjectPtr<AActor> InstigatorActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Shot")
	TObjectPtr<AActor> CauserActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Shot")
	FVector TraceStart = FVector::ZeroVector;

	// A 파트에서 Spread가 적용된 최종 방향을 펠릿 수만큼 전달한다.
	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Shot")
	TArray<FVector> PelletDirections;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Shot")
	FGameplayTag WeaponTag;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Shot")
	FGuid ShotId;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Shot", meta = (ClampMin = "0"))
	int32 ShotIndexInMagazine = 0;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Shot", meta = (ClampMin = "0.0"))
	float InitialPenetrationPower = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Shot")
	FHitscanDamageSettings DamageSettings;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Shot")
	FHitscanPenetrationSettings PenetrationSettings;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Shot")
	FHitscanTraceSettings TraceSettings;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Shot")
	TObjectPtr<UHitscanSurfaceDefinition> SurfaceDefinition = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Shot")
	TArray<TObjectPtr<AActor>> AdditionalIgnoredActors;
};

USTRUCT(BlueprintType)
struct FHitscanTraceSurfaceStep
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	TObjectPtr<AActor> SurfaceActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	TObjectPtr<UPrimitiveComponent> SurfaceComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	FVector EntryPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	FVector ExitPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	float ThicknessCentimeters = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	bool bUsedFallbackThickness = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	FHitscanPenetrationStepResult PenetrationResult;
};

USTRUCT(BlueprintType)
struct FHitscanPelletTraceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	int32 PelletIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	bool bProcessed = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	bool bHitDamageTarget = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	bool bStoppedBySurface = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	bool bStoppedByIterationLimit = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	float DistanceTraveledCentimeters = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	TArray<FHitscanImpactSample> ImpactSamples;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Trace")
	TArray<FHitscanTraceSurfaceStep> SurfaceSteps;
};

USTRUCT(BlueprintType)
struct FHitscanShotResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Shot")
	bool bProcessed = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Shot")
	FGuid ShotId;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Shot")
	TObjectPtr<AActor> InstigatorActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Shot")
	TObjectPtr<AActor> CauserActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Shot")
	FGameplayTag WeaponTag;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Shot")
	int32 ShotIndexInMagazine = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Shot")
	TArray<FHitscanPelletTraceResult> PelletResults;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Shot")
	FHitscanDamageCoreResult DamageCoreResult;
};
