#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DataJackInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS(BlueprintType)
class FPS_API UDataJackInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Input")
	int32 MappingPriority = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Input")
	TObjectPtr<UInputAction> DropAction;
};
