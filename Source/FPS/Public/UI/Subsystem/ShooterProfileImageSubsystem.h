// ShooterProfileImageSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ShooterProfileImageSubsystem.generated.h"

class UTexture2D;
struct FUniqueNetIdRepl;

// Steam 사용자 id 판정
struct FShooterProfileImageRules
{
	// Steam 개인 계정 id 가 아니면 0
	static uint64 ToSteamId(const FUniqueNetIdRepl& UniqueId);
};

/**
 * 플레이어 프로필 사진(Steam 아바타) 캐시
 */
UCLASS()
class FPS_API UShooterProfileImageSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UTexture2D* FindOrRequestProfileImage(const FUniqueNetIdRepl& UniqueId);

private:
	UPROPERTY(Transient)
	TMap<uint64, TObjectPtr<UTexture2D>> ImageBySteamId;

	TSet<uint64> RequestedSteamIds;
};
