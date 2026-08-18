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

	// 같은 BoneName이 여러 규칙에 들어간 경우 가장 높은 우선순위의 규칙을 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|HitZone")
	int32 Priority = 0;
};

UCLASS(BlueprintType)
class FPS_API UHitscanHitZoneDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	static FGameplayTag ResolveRules(
		const TArray<FHitscanHitZoneRule>& Rules,
		FName BoneName,
		FGameplayTag FallbackTag);

	UFUNCTION(BlueprintPure, Category = "FPS|Combat|Hitscan|HitZone")
	FGameplayTag ResolveHitZone(FName BoneName, FGameplayTag FallbackTag) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hitscan|HitZone", meta = (AllowPrivateAccess = "true"))
	TArray<FHitscanHitZoneRule> HitZoneRules;
};
