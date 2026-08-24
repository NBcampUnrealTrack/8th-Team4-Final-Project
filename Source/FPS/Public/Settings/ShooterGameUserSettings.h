// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "ShooterGameUserSettings.generated.h"

// 엔진 기본 GameUserSettings.ini 저장/로드 경로를 그대로 쓰는 프로젝트 전용 설정 클래스.
// Config/DefaultEngine.ini의 [/Script/Engine.Engine] GameUserSettingsClassName으로 등록되어야
// GEngine->GetGameUserSettings()가 이 클래스를 반환한다.
UCLASS()
class FPS_API UShooterGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	// GEngine->GetGameUserSettings()를 이 클래스로 캐스팅해 반환하는 편의 함수.
	UFUNCTION(BlueprintPure, Category = "FPS|Settings")
	static UShooterGameUserSettings* Get();

	// 크로스헤어가 실제 탄퍼짐(WeaponBase::GetCurrentSpreadDegrees)에 따라 벌어질지, 항상 최소
	// 크기로 고정될지. UShooterReticle이 매 프레임 이 값을 읽는다.
	UFUNCTION(BlueprintPure, Category = "FPS|Settings")
	bool IsDynamicCrosshairEnabled() const { return bDynamicCrosshair; }

	// 값을 바꾸고 즉시 저장(SaveSettings)까지 한다 - 설정 메뉴 체크박스든 콘솔 명령이든
	// 여기 하나만 호출하면 된다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Settings")
	void SetDynamicCrosshairEnabled(bool bEnabled);

protected:
	UPROPERTY(Config)
	bool bDynamicCrosshair = true;
};