// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Subsystems/WorldSubsystem.h"
#include "RogueRevealDebugDrawer.generated.h"

// 발각 시 디버그 드로잉
// 테스트를 위해 추가, 나중에 삭제
UCLASS()
class FPS_API URogueRevealDebugDrawer : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(URogueRevealDebugDrawer, STATGROUP_Tickables);
	}
};
