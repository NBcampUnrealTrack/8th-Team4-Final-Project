#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "HitscanShotTypes.generated.h"

class AActor;
class UHitscanSurfaceDefinition;

// 기존 HitscanWeaponDefinition DataAsset의 직렬화 경계를 유지하기 위해 이름은 보존한다.
// 신규 대미지 파이프라인에서는 절대 대미지를 계산하지 않고 부위/후방 판정 정책만 가진다.
USTRUCT(BlueprintType)
struct FHitscanDamageSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Hit")
	FGameplayTag DefaultHitZoneTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Hit")
	bool bEnableBackstab = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Hit", meta = (EditCondition = "bEnableBackstab", ClampMin = "0.0", ClampMax = "180.0"))
	float BackstabRearConeHalfAngleDegrees = 45.0f;
};

USTRUCT(BlueprintType)
struct FHitscanSurfaceRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration")
	bool bCanPenetrate = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageMultiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct FHitscanPenetrationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Penetration", meta = (ClampMin = "0", ClampMax = "64"))
	int32 MaximumPenetrationCount = 0;
};

USTRUCT(BlueprintType)
struct FHitscanDamageTargetSettings
{
	GENERATED_BODY()

	// 현재 RogueDamageSubsystem 계약상 ASC가 있는 액터만 실제 대미지 대상으로 처리한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Target")
	bool bRequireAbilitySystemTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Target")
	FName DamageableActorTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Target")
	bool bRequireDamageableActorTag = false;
};

USTRUCT(BlueprintType)
struct FHitscanTraceSettings
{
	GENERATED_BODY()

	// 프로젝트에서 전용 Weapon Trace 채널을 만들면 이 값을 해당 채널로 지정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	// BoneName 기반 부위 판정을 위해 기본값은 true로 둔다.
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace")
	bool bUsePenetrableSurfaceOverrides = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace")
	FHitscanSurfaceRule DefaultSurfaceRule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace")
	FHitscanDamageTargetSettings DamageTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Debug")
	bool bDrawDebugTrace = false;
};

struct FHitscanImpactSample
{
	TWeakObjectPtr<AActor> TargetActor;
	FVector ImpactPoint = FVector::ZeroVector;
	FVector ImpactNormal = FVector::UpVector;
	TEnumAsByte<EPhysicalSurface> ImpactSurfaceType = SurfaceType_Default;
	FName BoneName = NAME_None;
	FVector ShotDirection = FVector::ForwardVector;
	float WallRetention = 1.0f;
	int32 PelletIndex = 0;
	FGameplayTag HitZoneTag;
};

USTRUCT(BlueprintType)
struct FHitscanImpactEffectData
{
	GENERATED_BODY()

	// 서버 내부에서만 사용하는 피격 액터. FireEffects RPC에는 복제하지 않는다.
	TWeakObjectPtr<AActor> HitActor;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Effect")
	FVector ImpactPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Effect")
	FVector ImpactNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Effect")
	TEnumAsByte<EPhysicalSurface> ImpactSurfaceType = SurfaceType_Default;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Effect")
	int32 PelletIndex = 0;

	// 실제 표면에 맞은 경우에만 데칼/파티클/사운드 재생하도록 분기
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Effect")
	bool bHasHit = false;

	// 타겟이 임팩트 파티클을 허용하는지 서버에서 결정해 Multicast에 함께 전달한다.
	// HitActor는 서버 전용이므로 클라이언트에서 다시 타입을 판별하지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Effect")
	bool bSpawnImpactParticle = true;

	// 맞은 부위(헤드/토르소/팔다리 등). 헤드샷 여부 판정 및 헤드샷 전용 이펙트 분기에 쓰인다.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Combat|Hitscan|Effect")
	FGameplayTag HitZoneTag;
};

// 한 발의 실행 시점 데이터. 동기 C++ 호출 전용이라 Blueprint Reflection을 붙이지 않는다.
struct FHitscanShotRequest
{
	AActor* InstigatorActor = nullptr;
	AActor* CauserActor = nullptr;

	// 실제 조준/충돌 Trace의 시작점. 현재 FPS 카메라 위치를 사용한다.
	FVector TraceStart = FVector::ZeroVector;

	// GAS 거리 감쇠용 원점. 팀 계약에 따라 총구 위치를 사용한다.
	FVector DamageDistanceOrigin = FVector::ZeroVector;

	// Weapon이 만든 방향 배열을 동기 ResolveServerShot 호출 동안 참조한다.
	TConstArrayView<FVector> PelletDirections;

	// WeaponDamageData 조회에 사용하는 Weapon.Type.* 태그.
	FGameplayTag WeaponTag;
	FGuid ShotId;
	int32 ShotIndexInMagazine = 0;
	float ClientFireServerTime = 0.0f;

	// 이름은 기존 DataAsset 호환을 위해 유지하지만 부위/후방 판정 설정만 포함한다.
	FHitscanDamageSettings DamageSettings;
	FHitscanPenetrationSettings PenetrationSettings;
	FHitscanTraceSettings TraceSettings;
	UHitscanSurfaceDefinition* SurfaceDefinition = nullptr;
};
