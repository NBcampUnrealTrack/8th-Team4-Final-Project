// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AugmentRegistrySubsystem.generated.h"

class UAugmentDefinition;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAugmentRegistryReady);

// 한 계열의 레벨별 정의. Level 오름차순으로 정렬
USTRUCT()
struct FAugmentLine
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UAugmentDefinition>> ByLevel;
};

UCLASS()
class FPS_API UAugmentRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 카탈로그 로드 완료 여부. false 인 동안 조회는 전부 null
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	bool IsReady() const { return bReady; }

	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	UAugmentDefinition* FindDefinition(const FPrimaryAssetId& DefinitionId) const;

	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	TArray<UAugmentDefinition*> GetAllDefinitions() const;

	// 계열의 다음 레벨. 미보유는 CurrentLevel = 0
	// 최고 레벨이면 nullptr
	UFUNCTION(BlueprintPure, Category = "Rogue|Augment")
	UAugmentDefinition* FindNextLevelInLine(FGameplayTag LineTag, int32 CurrentLevel) const;

private:
	void StartAsyncLoad();
	void OnCatalogLoaded();
	void BuildIndex();

	// 증강 라인이 올바른지 검증. 잘못된 경우 로그를 남기고 무시
	static void ValidateLine(const FGameplayTag& LineTag, const FAugmentLine& Line);

	UPROPERTY()
	TMap<FPrimaryAssetId, TObjectPtr<UAugmentDefinition>> DefinitionsById;

	UPROPERTY()
	TMap<FGameplayTag, FAugmentLine> LinesByTag;

	TSharedPtr<FStreamableHandle> LoadHandle;

	bool bReady = false;

public:
	// 로드 완료 통보.
	UPROPERTY(BlueprintAssignable, Category = "Rogue|Augment")
	FOnAugmentRegistryReady OnRegistryReady;
};
