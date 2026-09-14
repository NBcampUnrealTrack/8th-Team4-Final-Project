#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterCustomizationComponent.generated.h"

class UCharacterFacePreset;
class UMaterialInstanceDynamic;
class UGroomAsset;
class UGroomBindingAsset;
class UGroomComponent;


UENUM(BlueprintType)
enum class ECharacterPresetType : uint8
{
	Seo		UMETA(DisplayName = "Seo"),
	Custom	UMETA(DisplayName = "Custom"),
	Fab		UMETA(DisplayName = "Fab")
};


USTRUCT(BlueprintType)
struct FPS_API FCharacterHairPreset
{
	GENERATED_BODY()

	// 헤어 Groom
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hair")
	TObjectPtr<UGroomAsset> Groom = nullptr;

	// Seo Face용 기존 Binding
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hair")
	TObjectPtr<UGroomBindingAsset> SeoBinding = nullptr;

	// Custom Face용 Binding
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hair")
	TObjectPtr<UGroomBindingAsset> CustomBinding = nullptr;

	// Fab Face용 Binding
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hair")
	TObjectPtr<UGroomBindingAsset> FabBinding = nullptr;
};


USTRUCT(BlueprintType)
struct FPS_API FCharacterCustomizationData
{
	GENERATED_BODY()

	/*
	 * 기존 호환용.
	 * 현재 Face Mesh 자체는 Mutable이 관리하므로
	 * 이 Preset은 Groom 계열의 기본 데이터 정도로만 사용한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization")
	TObjectPtr<UCharacterFacePreset> FacePreset = nullptr;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Customization",
		meta = (ClampMin = "0.0", ClampMax = "1.0")
	)
	float SkinValue = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Customization")
	FLinearColor OutfitColor = FLinearColor::White;
};


UCLASS(
	ClassGroup = (Customization),
	meta = (BlueprintSpawnableComponent)
)
class FPS_API UCharacterCustomizationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterCustomizationComponent();

	// 기존 FacePreset 호환용.
	// Face Mesh / Hair는 여기서 변경하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Customization")
	void ApplyFacePreset(UCharacterFacePreset* Preset);

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void ApplySkinValue(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void ApplyOutfitColor(const FLinearColor& NewColor);

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void ApplyCustomization(
		const FCharacterCustomizationData& Customization
	);

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SetSkinValue(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SetFacePreset(UCharacterFacePreset* NewPreset);

	UFUNCTION(BlueprintCallable, Category = "Customization")
	void SetOutfitColor(const FLinearColor& NewColor);


	// =========================
	// Hair
	// =========================

	UFUNCTION(BlueprintCallable, Category = "Customization|Hair")
	void SetHairStyle(int32 HairIndex);

	UFUNCTION(BlueprintCallable, Category = "Customization|Hair")
	void ApplyCurrentHair();

	/*
	 * Mutable의 BodyType과 반드시 같은 값으로 맞춰줘야 한다.
	 *
	 * Seo    -> Seo Binding
	 * Custom -> Custom Binding
	 * Fab    -> Fab Binding
	 */
	UFUNCTION(BlueprintCallable, Category = "Customization|Character")
	void SetCharacterPresetType(ECharacterPresetType NewPreset);


protected:
	virtual void BeginPlay() override;


	// =========================
	// 기존 Customization
	// =========================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Customization")
	FCharacterCustomizationData DefaultCustomization;

	UPROPERTY(BlueprintReadOnly, Category = "Customization")
	FCharacterCustomizationData CurrentCustomization;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FaceSkinMID = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodySkinMID = nullptr;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Customization|Outfit"
	)
	FName OutfitComponentName = TEXT("SkeletalMesh");

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> OutfitMIDs;


	// =========================
	// Hair Customization
	// =========================

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Customization|Hair"
	)
	FName HairComponentName = TEXT("Hair");

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Customization|Hair"
	)
	TArray<FCharacterHairPreset> HairPresets;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Customization|Character"
	)
	ECharacterPresetType CurrentCharacterPreset =
		ECharacterPresetType::Seo;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Customization|Hair"
	)
	int32 CurrentHairIndex = 0;

	UPROPERTY(Transient)
	TObjectPtr<UGroomComponent> HairComponent = nullptr;


private:
	void FindHairComponent();

	UGroomBindingAsset* GetBindingForCurrentCharacter(
		const FCharacterHairPreset& HairPreset
	) const;
};