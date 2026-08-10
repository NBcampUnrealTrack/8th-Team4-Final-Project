// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerCustomizationComponent.generated.h"

class UCustomizableObject;
class UCustomizableObjectInstance;
class UCustomizableSkeletalComponent;
class USkeletalMeshComponent;

UCLASS(ClassGroup = (Customization), meta = (BlueprintSpawnableComponent))
class FPS_API UPlayerCustomizationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerCustomizationComponent();

	// Attaches the customization to the given mesh (e.g. the character's body mesh) and creates the CO instance.
	UFUNCTION(BlueprintCallable, Category = "FPS|Customization")
	void InitializeForMeshComponent(USkeletalMeshComponent* TargetMesh);

	UFUNCTION(BlueprintCallable, Category = "FPS|Customization")
	void SetEnumOption(const FString& ParameterName, const FString& OptionName);

	UFUNCTION(BlueprintCallable, Category = "FPS|Customization")
	void SetBoolOption(const FString& ParameterName, bool bValue);

	UFUNCTION(BlueprintCallable, Category = "FPS|Customization")
	void SetColorOption(const FString& ParameterName, const FLinearColor& Value);

	// Rebuilds the skeletal mesh from the currently set parameters.
	UFUNCTION(BlueprintCallable, Category = "FPS|Customization")
	void ApplyCustomization();

protected:
	// CO_PlayerBase (or similar) Customizable Object asset defining the swappable parts and parameters.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Customization")
	TObjectPtr<UCustomizableObject> CustomizableObject;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Customization")
	TObjectPtr<UCustomizableSkeletalComponent> BodyCustomizationComponent;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "FPS|Customization")
	TObjectPtr<UCustomizableObjectInstance> CustomizableInstance;
};
