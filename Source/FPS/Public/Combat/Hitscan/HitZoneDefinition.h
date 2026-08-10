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

	// 탄환 Trace는 Capsule을 통과하고 Skeletal Mesh에 맞아 BoneName을 제공해야 한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|HitZone")
	TArray<FName> BoneNames;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|HitZone")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|HitZone")
	bool bCountsAsHeadshot = false;
};

struct FPS_API FHitscanResolvedHitZoneInfo
{
	FGameplayTag HitZoneTag;
	int32 Priority = 0;
	bool bHeadshot = false;
};

class FPS_API FHitscanHitZoneResolver final
{
public:
	static FHitscanResolvedHitZoneInfo Resolve(
		const TArray<FHitscanHitZoneRule>& Rules,
		FName BoneName,
		FGameplayTag FallbackTag);
};

UCLASS(BlueprintType)
class FPS_API UHitscanHitZoneDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// DataAsset의 Bone 규칙을 순수 Resolver로 전달해 Tag/Priority/Headshot을 함께 해석한다.
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
