#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterFacePreset.generated.h"

class USkeletalMesh;
class UGroomAsset;
class UGroomBindingAsset;

UCLASS(BlueprintType)
class FPS_API UCharacterFacePreset : public UDataAsset
{
	GENERATED_BODY()

public:

	// 倔奔
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face")
	TObjectPtr<USkeletalMesh> FaceMesh;

	// 赣府
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hair")
	TObjectPtr<UGroomAsset> HairGroom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hair")
	TObjectPtr<UGroomBindingAsset> HairBinding;

	// 传界
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyebrows")
	TObjectPtr<UGroomAsset> EyebrowsGroom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyebrows")
	TObjectPtr<UGroomBindingAsset> EyebrowsBinding;

	// 加传界
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyelashes")
	TObjectPtr<UGroomAsset> EyelashesGroom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eyelashes")
	TObjectPtr<UGroomBindingAsset> EyelashesBinding;

	// 荐堪
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Hair")
	TObjectPtr<UGroomAsset> BeardGroom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Hair")
	TObjectPtr<UGroomBindingAsset> BeardBinding;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Hair")
	TObjectPtr<UGroomAsset> MustacheGroom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Hair")
	TObjectPtr<UGroomBindingAsset> MustacheBinding;

	// 倔奔 儡判
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fuzz")
	TObjectPtr<UGroomAsset> FuzzGroom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fuzz")
	TObjectPtr<UGroomBindingAsset> FuzzBinding;
};