// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/NetworkTimeSyncComponent.h"

#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UNetworkTimeSyncComponent::UNetworkTimeSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UNetworkTimeSyncComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UNetworkTimeSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopTimeSync();

	Super::EndPlay(EndPlayReason);
}

void UNetworkTimeSyncComponent::StartTimeSync()
{
	if (!ShouldSyncTime())
	{
		return;
	}

	RequestServerTimeSync();

	if (TimeSyncFrequency <= 0.0f)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TimeSyncTimerHandle,
			this,
			&ThisClass::RequestServerTimeSync,
			TimeSyncFrequency,
			true
		);
	}
}

void UNetworkTimeSyncComponent::StopTimeSync()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimeSyncTimerHandle);
	}
}

void UNetworkTimeSyncComponent::RequestServerTimeSync()
{
	UWorld* World = GetWorld();
	if (!World || !ShouldSyncTime())
	{
		return;
	}

	if (const AActor* Owner = GetOwner())
	{
		if (Owner&&Owner->HasAuthority())
		{
			RoundTripTime = 0.0f;
			ServerTimeOffset = 0.0f;
			return;
		}
	}

	ServerRequestServerTime(World->GetTimeSeconds());
}

float UNetworkTimeSyncComponent::GetEstimatedServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		return World->GetTimeSeconds();
	}

	return World->GetTimeSeconds() + ServerTimeOffset;
}

void UNetworkTimeSyncComponent::ServerRequestServerTime_Implementation(float ClientRequestTime)
{
	const UWorld* World = GetWorld();
	float ServerReceiptTime = 0.0f;
	if (World)
	{
		ServerReceiptTime = World->GetTimeSeconds();
	}

	ClientReportServerTime(ClientRequestTime, ServerReceiptTime);
}	

void UNetworkTimeSyncComponent::ClientReportServerTime_Implementation(float ClientRequestTime, float ServerReceiptTime)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float ClientReceiveTime = World->GetTimeSeconds();
	RoundTripTime = FMath::Max(0.0f, ClientReceiveTime - ClientRequestTime);

	const float EstimatedServerTime = ServerReceiptTime + (RoundTripTime * 0.5f);
	ServerTimeOffset = EstimatedServerTime - ClientReceiveTime;

	UE_LOG(LogTemp, Warning, TEXT("[TimeSync] RTT=%.4f Offset=%.4f EstimatedServerTime=%.4f"),
		RoundTripTime,
		ServerTimeOffset,
		GetEstimatedServerTime());
}

bool UNetworkTimeSyncComponent::ShouldSyncTime() const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	return PlayerController && PlayerController->IsLocalController();
}
