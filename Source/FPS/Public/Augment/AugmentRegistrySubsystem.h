// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AugmentRegistrySubsystem.generated.h"

class UAugmentDefinition;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAugmentRegistryReady);

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

	// 애셋 하나가 계열 하나
	const TArray<TObjectPtr<UAugmentDefinition>>& GetAllDefinitions() const { return AllDefinitions; }

private:
	void StartAsyncLoad();
	void OnCatalogLoaded();
	void BuildIndex();

	UPROPERTY()
	TMap<FPrimaryAssetId, TObjectPtr<UAugmentDefinition>> DefinitionsById;

	UPROPERTY()
	TArray<TObjectPtr<UAugmentDefinition>> AllDefinitions;

	TSharedPtr<FStreamableHandle> LoadHandle;

	bool bReady = false;

public:
	// 로드 완료 통보.
	UPROPERTY(BlueprintAssignable, Category = "Rogue|Augment")
	FOnAugmentRegistryReady OnRegistryReady;
};
