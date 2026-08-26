// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "ShooterGameMode.generated.h"

class AController;
class APlayerController;
class AShooterPlayerState;

/**
 * 
 */
UCLASS()
class FPS_API AShooterGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AShooterGameMode();

	// Character의 서버 사망 통지를 실제 게임 규칙으로 전달하는 공통 진입점이다.
	virtual void NotifyPlayerEliminatedBy(
		AController* EliminatedController,
		AShooterPlayerState* KillerPlayerState);

	// 로컬 플레이어의 맵/UI 준비 완료를 모드별 시작 게이트로 전달한다.
	virtual void NotifyPlayerReadyForGameFlow(APlayerController* PlayerController);
	
};
