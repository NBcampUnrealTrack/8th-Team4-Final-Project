#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "Combat/Hitscan/HitscanTypes.h"
#include "HitscanShotTypes.generated.h"

class AActor;
class UHitscanSurfaceDefinition;

USTRUCT(BlueprintType)
struct FHitscanDamageTargetSettings
{
	GENERATED_BODY()

	// 현재 RogueDamageSubsystem 계약상 ASC가 있는 액터만 실제 대미지 대상으로 처리한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Target")
	bool bRequireAbilitySystemTarget = true;

	// ASC 없는 별도 타깃을 허용할 때만 BoneName을 후보 판정에 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Target", meta = (EditCondition = "!bRequireAbilitySystemTarget"))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace", meta = (ClampMin = "1", ClampMax = "128"))
	int32 MaximumPelletCount = 64;

	// 출구 역트레이스 실패 시 AABB 출구 거리 이상으로 보수적 fallback을 허용할지 결정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace")
	bool bAllowFallbackSurfaceThickness = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Trace", meta = (EditCondition = "bAllowFallbackSurfaceThickness", ClampMin = "0.1"))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Combat|Hitscan|Debug")
	bool bLogResolvedDamage = false;
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
	bool bHasDamageDistanceOrigin = false;

	// Weapon이 만든 방향 배열을 동기 ResolveServerShot 호출 동안 참조한다.
	TConstArrayView<FVector> PelletDirections;

	// WeaponDamageData 조회에 사용하는 Weapon.Type.* 태그.
	FGameplayTag WeaponTag;
	FGuid ShotId;
	int32 ShotIndexInMagazine = 0;
	float ClientFireServerTime = 0.0f;
	float InitialPenetrationPower = 0.0f;

	// 이름은 기존 DataAsset 호환을 위해 유지하지만 부위/후방 판정 설정만 포함한다.
	FHitscanDamageSettings DamageSettings;
	FHitscanPenetrationSettings PenetrationSettings;
	FHitscanTraceSettings TraceSettings;
	UHitscanSurfaceDefinition* SurfaceDefinition = nullptr;

	// Weapon이 소유하는 맵을 동기 호출 동안 참조해 발사마다 TMap을 복사하지 않는다.
	const TMap<TEnumAsByte<EPhysicalSurface>, float>* SurfaceHardnessOverrides = nullptr;
};

// 외부 호출자는 성공 여부와 GAS 적용 개수만 확인한다.
struct FHitscanShotResult
{
	bool bProcessed = false;
	FGuid ShotId;
	int32 ResolvedTargetCount = 0;
	int32 AppliedTargetCount = 0;
};
