// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PenetrableSurfaceComponent.generated.h"

// 레벨에 배치된 벽/오브젝트 액터에 붙여서 관통 두께를 지정하는 마커 컴포넌트.
// AWeaponBase가 벽 관통 판정 시 이 컴포넌트를 찾아 두께 값을 읽는다.
// 이 컴포넌트가 없는 액터는 AWeaponBase의 DefaultSurfaceThickness 값이 대신 사용된다.
//
// TODO(팀 협의 필요 - 투사체/관통 연산 담당자): 이 컴포넌트는 히트스캔 무기(AWeaponBase)의
// "벽 관통 판정" 요구사항을 충족하기 위한 최소 구현이다. AProjectileBase(투사체) 쪽 관통 연산도
// 벽 두께 데이터가 필요하다면, 레벨에 두께를 두 번 따로 태깅하는 대신 이 컴포넌트를 공용으로
// 쓸지(혹은 공용 위치로 옮길지) 담당자와 반드시 협의할 것.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FPS_API UPenetrableSurfaceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPenetrableSurfaceComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Weapon|Penetration")
	float PenetrationThickness = 10.f;
};