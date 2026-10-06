#include "Animation/CharacterCustomizationComponent.h"

#include "Animation/CharacterFacePreset.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "GroomComponent.h"

namespace
{
	const FName SkinValueParameterName(TEXT("Basecolor Global Value Post-Bake"));
	const FName OutfitColorParameterName(TEXT("diffuse_color_1"));

	template <typename T>
	T* FindComponentByName(AActor* Owner, const FName ComponentName)
	{
		if (!IsValid(Owner))
		{
			return nullptr;
		}

		TArray<T*> Components;
		Owner->GetComponents<T>(Components);

		for (T* Component : Components)
		{
			if (IsValid(Component) && Component->GetFName() == ComponentName)
			{
				return Component;
			}
		}

		return nullptr;
	}

	void ApplyGroom(
		UGroomComponent* Component,
		UGroomAsset* Groom,
		UGroomBindingAsset* Binding)
	{
		if (!IsValid(Component))
		{
			return;
		}

		// A successful gameplay appearance replacement retires authored grooms.
		// Late legacy preset calls must not show them over the selected face.
		if (Component->ComponentHasTag(TEXT("AppearanceNoOutline")))
		{
			return;
		}

		if (!IsValid(Groom))
		{
			Component->SetVisibility(false);
			return;
		}

		Component->SetVisibility(true);
		Component->SetGroomAsset(Groom, Binding, true);
	}
}

UCharacterCustomizationComponent::UCharacterCustomizationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCharacterCustomizationComponent::BeginPlay()
{
	Super::BeginPlay();

	ApplyCustomization(DefaultCustomization);
	FindHairComponent();
	ApplyCurrentHair();
}

void UCharacterCustomizationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	CancelPendingHair();
	Super::EndPlay(EndPlayReason);
}

void UCharacterCustomizationComponent::CancelPendingHair()
{
	// Invalidate even callbacks already queued for delivery before cancelling the handle.
	++HairRequestSerial;
	if (HairLoadHandle) HairLoadHandle->CancelHandle();
	HairLoadHandle.Reset();
}

void UCharacterCustomizationComponent::ApplyFacePreset(UCharacterFacePreset* Preset)
{
	if (!IsValid(Preset))
	{
		return;
	}

	AActor* Owner = GetOwner();

	if (!IsValid(Owner))
	{
		return;
	}

	ApplyGroom(
		FindComponentByName<UGroomComponent>(Owner, TEXT("Eyebrows")),
		Preset->EyebrowsGroom,
		Preset->EyebrowsBinding);

	ApplyGroom(
		FindComponentByName<UGroomComponent>(Owner, TEXT("Eyelashes")),
		Preset->EyelashesGroom,
		Preset->EyelashesBinding);

	ApplyGroom(
		FindComponentByName<UGroomComponent>(Owner, TEXT("Beard")),
		Preset->BeardGroom,
		Preset->BeardBinding);

	ApplyGroom(
		FindComponentByName<UGroomComponent>(Owner, TEXT("Mustache")),
		Preset->MustacheGroom,
		Preset->MustacheBinding);

	ApplyGroom(
		FindComponentByName<UGroomComponent>(Owner, TEXT("Fuzz")),
		Preset->FuzzGroom,
		Preset->FuzzBinding);
}

void UCharacterCustomizationComponent::ApplySkinValue(const float NewValue)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());

	if (!IsValid(Character))
	{
		return;
	}

	USkeletalMeshComponent* Face =
		FindComponentByName<USkeletalMeshComponent>(Character, TEXT("Face"));

	if (IsValid(Face))
	{
		if (!IsValid(FaceSkinMID))
		{
			FaceSkinMID = Face->CreateDynamicMaterialInstance(0);
		}

		if (IsValid(FaceSkinMID))
		{
			FaceSkinMID->SetScalarParameterValue(
				SkinValueParameterName,
				NewValue);
		}
	}

	USkeletalMeshComponent* Body = Character->GetMesh();

	if (IsValid(Body))
	{
		if (!IsValid(BodySkinMID))
		{
			BodySkinMID = Body->CreateDynamicMaterialInstance(0);
		}

		if (IsValid(BodySkinMID))
		{
			BodySkinMID->SetScalarParameterValue(
				SkinValueParameterName,
				NewValue);
		}
	}
}

void UCharacterCustomizationComponent::ApplyOutfitColor(
	const FLinearColor& NewColor)
{
	AActor* Owner = GetOwner();

	if (!IsValid(Owner))
	{
		return;
	}

	USkeletalMeshComponent* OutfitMesh =
		FindComponentByName<USkeletalMeshComponent>(
			Owner,
			OutfitComponentName);

	if (!IsValid(OutfitMesh))
	{
		return;
	}

	const int32 MaterialCount = OutfitMesh->GetNumMaterials();

	if (OutfitMIDs.Num() != MaterialCount)
	{
		OutfitMIDs.Empty();
		OutfitMIDs.SetNum(MaterialCount);

		for (int32 Index = 0; Index < MaterialCount; ++Index)
		{
			OutfitMIDs[Index] =
				OutfitMesh->CreateDynamicMaterialInstance(Index);
		}
	}

	for (UMaterialInstanceDynamic* MID : OutfitMIDs)
	{
		if (IsValid(MID))
		{
			MID->SetVectorParameterValue(
				OutfitColorParameterName,
				NewColor);
		}
	}
}

void UCharacterCustomizationComponent::ApplyCustomization(
	const FCharacterCustomizationData& Customization)
{
	CurrentCustomization = Customization;

	if (IsValid(Customization.FacePreset))
	{
		ApplyFacePreset(Customization.FacePreset);
	}

	ApplySkinValue(Customization.SkinValue);
	ApplyOutfitColor(Customization.OutfitColor);
}

void UCharacterCustomizationComponent::SetSkinValue(const float NewValue)
{
	CurrentCustomization.SkinValue = NewValue;
	ApplySkinValue(NewValue);
}

void UCharacterCustomizationComponent::SetFacePreset(
	UCharacterFacePreset* NewPreset)
{
	if (!IsValid(NewPreset))
	{
		return;
	}

	CurrentCustomization.FacePreset = NewPreset;
	ApplyFacePreset(NewPreset);
}

void UCharacterCustomizationComponent::SetOutfitColor(
	const FLinearColor& NewColor)
{
	CurrentCustomization.OutfitColor = NewColor;
	ApplyOutfitColor(NewColor);
}

void UCharacterCustomizationComponent::FindHairComponent()
{
	HairComponent = nullptr;

	AActor* Owner = GetOwner();

	if (!IsValid(Owner))
	{
		return;
	}

	TArray<UGroomComponent*> GroomComponents;
	Owner->GetComponents<UGroomComponent>(GroomComponents);

	for (UGroomComponent* GroomComponent : GroomComponents)
	{
		if (!IsValid(GroomComponent))
		{
			continue;
		}

		if (GroomComponent->GetFName() == HairComponentName)
		{
			HairComponent = GroomComponent;
			return;
		}
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("CharacterCustomization: Hair Groom Component '%s' not found."),
		*HairComponentName.ToString());
}

TSoftObjectPtr<UGroomBindingAsset>
UCharacterCustomizationComponent::GetBindingForCurrentCharacter(
	const FCharacterHairPreset& HairPreset) const
{
	switch (CurrentCharacterPreset)
	{
	case ECharacterPresetType::Seo:
		return HairPreset.SeoBinding;

	case ECharacterPresetType::Custom:
		return HairPreset.CustomBinding;

	case ECharacterPresetType::Fab:
		return HairPreset.FabBinding;

	default:
		return nullptr;
	}
}

void UCharacterCustomizationComponent::ApplyCurrentHair()
{
	CancelPendingHair();
	if (bEndingPlay) return;
	if (!IsValid(HairComponent))
	{
		FindHairComponent();
	}

	if (!IsValid(HairComponent))
	{
		return;
	}

	if (HairPresets.Num() == 0)
	{
		return;
	}

	// Keep legacy hair as a fallback until replacement succeeds, but never
	// re-enable it afterward (BeginPlay and asynchronous completion can race).
	if (HairComponent->ComponentHasTag(TEXT("AppearanceNoOutline")))
	{
		return;
	}

	if (!HairPresets.IsValidIndex(CurrentHairIndex))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("CharacterCustomization: Invalid HairIndex %d"),
			CurrentHairIndex);

		return;
	}

	const FCharacterHairPreset& HairPreset =
		HairPresets[CurrentHairIndex];

	if (HairPreset.Groom.IsNull())
	{
		HairComponent->SetVisibility(false);
		return;
	}

	const TSoftObjectPtr<UGroomBindingAsset> Binding =
		GetBindingForCurrentCharacter(HairPreset);

	if (Binding.IsNull())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"CharacterCustomization: No valid Hair Binding. "
				"Hair=%d CharacterPreset=%d"),
			CurrentHairIndex,
			static_cast<int32>(CurrentCharacterPreset));

		return;
	}

	const TSoftObjectPtr<UGroomAsset> Groom = HairPreset.Groom;
	const uint64 Request = HairRequestSerial;
	const TWeakObjectPtr<UGroomComponent> RequestedComponent = HairComponent;
	TArray<FSoftObjectPath> Paths{Groom.ToSoftObjectPath(), Binding.ToSoftObjectPath()};
	// Stalling lets us own the handle before an already-resident request can finish.
	HairLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
		FStreamableDelegate::CreateWeakLambda(this, [this, Request, RequestedComponent, Groom, Binding]
		{
			if (bEndingPlay || Request != HairRequestSerial) return;
			auto* Component = RequestedComponent.Get();
			// Mutable can replace/retire authored hair while this legacy load is in flight.
			if (!Component || Component != HairComponent || Component->ComponentHasTag(TEXT("AppearanceNoOutline")))
			{
				HairLoadHandle.Reset();
				return;
			}
			auto* LoadedGroom = Groom.Get();
			auto* LoadedBinding = Binding.Get();
			auto* Face = Cast<USkeletalMeshComponent>(Component->GetAttachParent());
			const bool bCompatible = LoadedGroom && LoadedBinding
				&& LoadedGroom->IsValid()
				&& UGroomBindingAsset::IsBindingAssetValid(LoadedBinding, false, false)
				&& UGroomBindingAsset::IsCompatible(LoadedGroom, LoadedBinding, false)
				&& (!Face || UGroomBindingAsset::IsCompatible(Face->GetSkeletalMeshAsset(), LoadedBinding, false));
			if (bCompatible)
			{
				Component->SetGroomAsset(LoadedGroom, LoadedBinding, true);
				Component->SetVisibility(true);
			}
			else
			{
				// Keep the previous fallback visible if loading or compatibility validation fails.
				UE_LOG(LogTemp, Warning, TEXT("CharacterCustomization: Selected hair failed to load or has incompatible binding: %s"), *Groom.ToString());
			}
			HairLoadHandle.Reset();
		}), FStreamableManager::DefaultAsyncLoadPriority, false, true, TEXT("Legacy selected hair"));
	if (HairLoadHandle) HairLoadHandle->StartStalledHandle();
}

void UCharacterCustomizationComponent::SetHairStyle(const int32 HairIndex)
{
	if (!HairPresets.IsValidIndex(HairIndex))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("CharacterCustomization: HairIndex %d is out of range."),
			HairIndex);

		return;
	}

	CurrentHairIndex = HairIndex;
	ApplyCurrentHair();
}

void UCharacterCustomizationComponent::SetCharacterPresetType(
	const ECharacterPresetType NewPreset)
{
	CurrentCharacterPreset = NewPreset;
	ApplyCurrentHair();
}
