#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "HitZoneDefinition.generated.h"

USTRUCT(BlueprintType)
struct FHitscanHitZoneRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|HitZone")
	FGameplayTag HitZoneTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|HitZone")
	TArray<FName> BoneNames;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|HitZone")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|HitZone")
	bool bCountsAsHeadshot = false;
};

struct FHitscanResolvedHitZoneInfo
{
	FGameplayTag HitZoneTag;
	int32 Priority = 0;
	bool bHeadshot = false;
};

UCLASS(BlueprintType)
class FPS_API UHitscanHitZoneDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	static FHitscanResolvedHitZoneInfo ResolveRules(
		const TArray<FHitscanHitZoneRule>& Rules,
		FName BoneName,
		FGameplayTag FallbackTag);

	FHitscanResolvedHitZoneInfo ResolveHitZoneInfo(FName BoneName, FGameplayTag FallbackTag) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Combat|Hitscan|HitZone")
	FGameplayTag ResolveHitZone(FName BoneName, FGameplayTag FallbackTag) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Combat|Hitscan|HitZone")
	int32 GetHitZonePriority(FGameplayTag HitZoneTag) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Combat|Hitscan|HitZone")
	bool IsHeadshotZone(FGameplayTag HitZoneTag) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|HitZone", meta = (AllowPrivateAccess = "true"))
	TArray<FHitscanHitZoneRule> HitZoneRules;
};
