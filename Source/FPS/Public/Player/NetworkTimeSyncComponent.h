// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "NetworkTimeSyncComponent.generated.h"

UCLASS(ClassGroup=(Network), meta=(BlueprintSpawnableComponent))
class FPS_API UNetworkTimeSyncComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNetworkTimeSyncComponent();

	UFUNCTION(BlueprintCallable, Category = "Shooter|Network Time")
	void StartTimeSync();

	UFUNCTION(BlueprintCallable, Category = "Shooter|Network Time")
	void StopTimeSync();

	UFUNCTION(BlueprintCallable, Category = "Shooter|Network Time")
	void RequestServerTimeSync();

	UFUNCTION(BlueprintPure, Category = "Shooter|Network Time")
	float GetEstimatedServerTime() const;

	UFUNCTION(BlueprintPure, Category = "Shooter|Network Time")
	float GetServerTimeOffset() const { return ServerTimeOffset; }

	UFUNCTION(BlueprintPure, Category = "Shooter|Network Time")
	float GetRoundTripTime() const { return RoundTripTime; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(Server, Reliable)
	void ServerRequestServerTime(float ClientRequestTime);

	UFUNCTION(Client, Reliable)
	void ClientReportServerTime(float ClientRequestTime, float ServerReceiptTime);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Network Time", meta = (ClampMin = "0.1"))
	float TimeSyncFrequency = 5.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Shooter|Network Time")
	float ServerTimeOffset = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Shooter|Network Time")
	float RoundTripTime = 0.0f;

private:
	bool ShouldSyncTime() const;

	FTimerHandle TimeSyncTimerHandle;
};
