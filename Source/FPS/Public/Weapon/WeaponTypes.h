// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WeaponTypes.generated.h"

class AActor;

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