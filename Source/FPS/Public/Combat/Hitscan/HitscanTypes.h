#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "HitscanTypes.generated.h"

class AActor;
class UCurveFloat;
class UHitscanHitZoneDefinition;

UENUM(BlueprintType)
enum class EHitscanDamageRoundingMode : uint8
{
	None,
	Nearest,
	Floor,
	Ceil
};

USTRUCT(BlueprintType)
struct FHitscanDistanceFalloffSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage", meta = (ClampMin = "0.0"))
	float EffectiveRangeMeters = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage", meta = (ClampMin = "0.0"))
	float MaximumRangeMeters = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumDamageMultiplierAtMaximumRange = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	TObjectPtr<UCurveFloat> DamageMultiplierCurve = nullptr;
};

USTRUCT(BlueprintType)
struct FHitscanDamageRoundingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	EHitscanDamageRoundingMode Mode = EHitscanDamageRoundingMode::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage", meta = (ClampMin = "0.0001"))
	float Step = 0.01f;
};

USTRUCT(BlueprintType)
struct FHitscanDamageSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage", meta = (ClampMin = "0.0"))
	float BaseDamagePerPellet = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	FHitscanDistanceFalloffSettings DistanceFalloff;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	TObjectPtr<UHitscanHitZoneDefinition> HitZoneDefinition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	TMap<FGameplayTag, float> HitZoneDamageMultipliers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	FGameplayTag DefaultHitZoneTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	FGameplayTag HeadshotHitZoneTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	bool bEnableBackstab = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage", meta = (EditCondition = "bEnableBackstab", ClampMin = "0.0", ClampMax = "180.0"))
	float BackstabRearConeHalfAngleDegrees = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Damage")
	FHitscanDamageRoundingSettings Rounding;
};

USTRUCT(BlueprintType)
struct FHitscanSurfaceRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0.0"))
	float Hardness = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0.0"))
	float PenetrationCostPerCentimeter = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageRetentionPerCentimeter = 1.0f;
};

USTRUCT(BlueprintType)
struct FHitscanPenetrationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0", ClampMax = "64"))
	int32 MaximumPenetrationCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumDamageMultiplierToContinue = 0.0f;
};

USTRUCT(BlueprintType)
struct FHitscanPenetrationState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration")
	float RemainingPenetrationPower = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration")
	float AccumulatedDamageMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration")
	int32 PenetrationCount = 0;
};

USTRUCT(BlueprintType)
struct FHitscanSurfaceSample
{
	GENERATED_BODY()

	// 실제 Trace의 진입점과 출구점으로 측정한 두께를 전달한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0.0"))
	float ThicknessCentimeters = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration")
	FHitscanSurfaceRule SurfaceRule;
};

USTRUCT(BlueprintType)
struct FHitscanPenetrationStepResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration")
	bool bPenetrated = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration")
	float PenetrationCost = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration")
	float SurfaceDamageMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration")
	FHitscanPenetrationState ResultingState;
};

USTRUCT(BlueprintType)
struct FHitscanPenetrationSequenceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration")
	bool bReachedEnd = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration")
	FHitscanPenetrationState FinalState;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Penetration")
	TArray<FHitscanPenetrationStepResult> Steps;
};

USTRUCT(BlueprintType)
struct FHitscanImpactSample
{
	GENERATED_BODY()

	// 서버에서 승인된 Hit 결과를 Resolver 또는 경계 Adapter가 이 중립 구조로 변환한다.
	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan")
	FVector ImpactPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan")
	FVector ImpactNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan")
	FName BoneName = NAME_None;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan")
	FVector ShotDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan", meta = (ClampMin = "0.0"))
	float DistanceMeters = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PenetrationDamageMultiplier = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan", meta = (ClampMin = "0"))
	int32 PelletIndex = 0;
};

USTRUCT(BlueprintType)
struct FHitscanResolvedPelletDamage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	FGameplayTag HitZoneTag;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	float DamageBeforeHitZoneMultiplier = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	float FinalDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	float HitZoneMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	float DistanceMeters = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	int32 HitZonePriority = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	int32 PelletIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	bool bHeadshot = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	bool bBackstab = false;
};

USTRUCT(BlueprintType)
struct FHitscanTargetDamageAggregate
{
	GENERATED_BODY()

	// GAS 연동 계약상 한 Shot에서 같은 Target에 맞은 펠릿은 이 결과 1건으로 묶어 ApplyDamage를 1회 호출한다.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	float BaseDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	float HeadshotDamage = 0.0f;

	// HeadshotDamage에 이미 적용된 무기 부위 배율. GAS가 헤드샷 보너스를 분리/복원할 때 사용한다.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	float AppliedHeadshotMultiplier = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	FGameplayTag RepresentativeHitZoneTag;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	float DistanceMeters = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	int32 PelletsHit = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	bool bIsBackstab = false;
};

USTRUCT(BlueprintType)
struct FHitscanDamageCoreRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan")
	TArray<FHitscanImpactSample> ImpactSamples;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan")
	FHitscanDamageSettings DamageSettings;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|Combat|Hitscan", meta = (ClampMin = "1"))
	int32 ExpectedPelletCount = 1;
};

USTRUCT(BlueprintType)
struct FHitscanDamageCoreResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	bool bProcessed = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	int32 ExpectedPelletCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	int32 ResolvedImpactCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	TArray<FHitscanResolvedPelletDamage> ResolvedPellets;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan")
	TArray<FHitscanTargetDamageAggregate> TargetAggregates;
};
