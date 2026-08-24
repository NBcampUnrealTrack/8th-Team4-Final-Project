#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CombatHitZoneDefinition.generated.h"

USTRUCT(BlueprintType)
struct FCombatHitBoxDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hit Zone")
	FName HitBoxName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hit Zone")
	FName AttachBoneName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hit Zone")
	FVector BoxExtent = FVector(10.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hit Zone")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hit Zone")
	FRotator RelativeRotation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct FCombatHitZoneRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hit Zone")
	FGameplayTag HitZoneTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hit Zone")
	TArray<FName> BoneNames;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hit Zone")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hit Zone")
	TArray<FCombatHitBoxDefinition> HitBoxes;
};

UCLASS(BlueprintType)
class FPS_API UCombatHitZoneDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	static FGameplayTag ResolveRules(
		const TArray<FCombatHitZoneRule>& Rules,
		FName BoneName,
		FGameplayTag FallbackTag);

	UFUNCTION(BlueprintPure, Category = "FPS|Combat|Hit Zone")
	FGameplayTag ResolveHitZone(FName BoneName, FGameplayTag FallbackTag) const;

	const TArray<FCombatHitZoneRule>& GetZoneRules() const { return ZoneRules; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Combat|Hit Zone", meta = (AllowPrivateAccess = "true"))
	TArray<FCombatHitZoneRule> ZoneRules;
};
