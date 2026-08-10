// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/PlayerCustomizationComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"

UPlayerCustomizationComponent::UPlayerCustomizationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	BodyCustomizationComponent = CreateDefaultSubobject<UCustomizableSkeletalComponent>(TEXT("BodyCustomizationComponent"));
}

void UPlayerCustomizationComponent::InitializeForMeshComponent(USkeletalMeshComponent* TargetMesh)
{
	if (!CustomizableObject || !TargetMesh)
	{
		return;
	}

	CustomizableInstance = CustomizableObject->CreateInstance();
	if (!CustomizableInstance)
	{
		return;
	}

	BodyCustomizationComponent->SetCustomizableObjectInstance(CustomizableInstance);
	BodyCustomizationComponent->AttachToComponent(TargetMesh, FAttachmentTransformRules::KeepRelativeTransform);

	ApplyCustomization();
}

void UPlayerCustomizationComponent::SetEnumOption(const FString& ParameterName, const FString& OptionName)
{
	if (CustomizableInstance)
	{
		CustomizableInstance->SetIntParameterSelectedOption(ParameterName, OptionName);
	}
}

void UPlayerCustomizationComponent::SetBoolOption(const FString& ParameterName, bool bValue)
{
	if (CustomizableInstance)
	{
		CustomizableInstance->SetBoolParameterSelectedOption(ParameterName, bValue);
	}
}

void UPlayerCustomizationComponent::SetColorOption(const FString& ParameterName, const FLinearColor& Value)
{
	if (CustomizableInstance)
	{
		CustomizableInstance->SetVectorParameterSelectedOption(ParameterName, Value);
	}
}

void UPlayerCustomizationComponent::ApplyCustomization()
{
	if (CustomizableInstance)
	{
		CustomizableInstance->UpdateSkeletalMeshAsync();
	}
}
