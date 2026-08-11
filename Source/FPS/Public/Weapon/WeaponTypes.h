// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "WeaponTypes.generated.h"

class AActor;
class UCurveFloat;

UENUM(BlueprintType)
enum class EWeaponSlot : uint8
{
	None UMETA(Hidden),
	Primary UMETA(DisplayName = "Primary"),
	Secondary UMETA(DisplayName = "Secondary")
};

// AWeaponBase가 외부(다른 팀원의 GAS Adapter 등)에 노출하는 순수 사격 결과 데이터.
// GAS 타입(FGameplayTag, AbilitySystemComponent 등)을 절대 포함하지 않는다.
// 다른 팀원이 이 구조체를 FRogueDamageRequest로 변환하는 Adapter를 별도로 작성할 예정이므로,
// 지금 당장 쓰이지 않는 필드(HitLocation, HitAngleDegrees, RemainingPenetrationPower 등)도 항상 채워서 내보낸다.
USTRUCT(BlueprintType)
struct FWeaponHitResult
{
	GENERATED_BODY()

	// 발사한 무기의 소유 액터(사격자)
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon")
	TObjectPtr<AActor> SourceActor = nullptr;

	// 이번 히트로 맞은 액터 (벽이면 벽 액터, 캐릭터면 해당 캐릭터)
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon")
	TObjectPtr<AActor> TargetActor = nullptr;

	// 관통 감쇄가 적용된, 이 히트 시점의 대미지
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon")
	float BaseDamage = 0.f;

	// 사격 위치로부터 이 히트까지의 거리 (m)
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon")
	float DistanceMeters = 0.f;

	// 히트 위치 원본 벡터. GAS 쪽이 HitZoneTag로 변환하든 안 하든 항상 채운다.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon")
	FVector HitLocation = FVector::ZeroVector;

	// 타겟의 정면 벡터 대비 입사각(도). 백스탭 판정 외 용도 대비, 항상 채운다.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon")
	float HitAngleDegrees = 0.f;

	// 피격 본 이름 (FHitResult::BoneName 원본). 데미지 파이프라인이 HitLocation만으로
	// 부위(헤드/상체/사지)를 추정하는 것보다 정확하게 판정할 수 있도록 함께 넘긴다.
	// 엔진 기본 타입(FName)이라 GAS와 무관하다.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon")
	FName HitBoneName = NAME_None;

	// 이 히트 지점을 관통해서 트레이스가 계속되었는지 여부
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon")
	bool bPenetratedWall = false;

	// 이 히트 이후 남은 관통력 (데미지 감쇠 계산용, 사용 안 해도 채워둔다)
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon")
	float RemainingPenetrationPower = 0.f;

	// 샷건 등 멀티 펠릿 대응, 단일 발사면 0
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon")
	int32 PelletIndex = 0;
};

// AWeaponBase가 사격 히트를 알릴 때 사용하는 델리게이트.
// 다른 팀원의 GAS Adapter가 이 델리게이트를 구독해서 FRogueDamageRequest 등으로 변환한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponHit, const FWeaponHitResult&, HitResult);

// 탄퍼짐/반동 튜닝 값 한 세트. 기획(밸런스) 담당자가 코드/블루프린트를 열지 않고도
// DataTable 행만 수정해서 에디터에서 바로 테스트할 수 있도록 분리한 값들이다.
// AWeaponBase::SpreadRecoilTable + SpreadRecoilRowName이 지정되면 BeginPlay 시 이 값들로
// BaseSpreadDegrees 등 개별 프로퍼티를 덮어쓴다. 미지정 시에는 무기 블루프린트에 직접
// 설정된 기존 값을 그대로 쓴다(하위 호환 fallback).
USTRUCT(BlueprintType)
struct FWeaponSpreadRecoilRow : public FTableRowBase
{
	GENERATED_BODY()

	// --- 탄퍼짐 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Spread")
	float BaseSpreadDegrees = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Spread")
	float BloomPerShotDegrees = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Spread")
	float MaxBloomDegrees = 5.f;

	// 사격을 멈춘 뒤 이 시간(초)이 지나야 ConsecutiveShotCount 감소가 시작된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Spread")
	float SpreadDecayDelay = 0.3f;

	// 초당 감소하는 연속 사격 카운트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Spread")
	float ConsecutiveShotDecayPerSecond = 5.f;

	// --- 반동 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Recoil")
	TObjectPtr<UCurveFloat> RecoilCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Recoil")
	float RecoilYawFactor = 0.f;

	// 사격 중단 후 이 시간(초)이 지나야 반동 복귀가 시작된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Recoil")
	float RecoilRecoveryDelay = 0.15f;

	// 반동 복귀 FInterpTo 속도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FPS|Weapon|Recoil")
	float RecoilRecoverySpeed = 10.f;
};