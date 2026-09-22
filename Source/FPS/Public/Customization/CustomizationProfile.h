#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CustomizationProfile.generated.h"

USTRUCT(BlueprintType)
struct FPS_API FCustomizationAppearance
{
 GENERATED_BODY()
 FCustomizationAppearance();
 UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FString BodyType = TEXT("Seo");
 UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName HairId = TEXT("Default");
 UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FLinearColor HairColor = FLinearColor::White;
 // Missing fields in v1 saves retain the authored material, not a white dye.
 UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bUseOriginalHairColor = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FLinearColor EyeColor = FLinearColor::White;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FLinearColor SkinColor = FLinearColor::White;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float SkinValue = 1.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FLinearColor> Colors;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<float> Values;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<float> Originals;
 bool Validate() const;
 bool ValidateForClass(const FString& ClassId) const;
 static const TArray<FString>& Parts();
};

UCLASS()
class FPS_API UCustomizationProfile : public USaveGame
{
 GENERATED_BODY()
public:
 UPROPERTY(SaveGame) int32 Version = 1;
 UPROPERTY(SaveGame) FString Service;
 UPROPERTY(SaveGame) FString Account;
 UPROPERTY(SaveGame) FString CharacterClass = TEXT("Ranger");
 UPROPERTY(SaveGame) FCustomizationAppearance Appearance;
};

enum class ECustomizationLoadResult { Loaded, Missing, Invalid };
struct FPS_API FCustomizationStorage
{
 static FString SlotFor(const FString& Service,const FString& Account,const FString& CharacterClass);
 static bool Matches(const UCustomizationProfile* Profile,const FString& Service,const FString& Account,const FString& CharacterClass);
 static ECustomizationLoadResult Load(const FString& Service,const FString& Account,const FString& CharacterClass,FCustomizationAppearance& Out);
 static bool Save(const FString& Service,const FString& Account,const FString& CharacterClass,const FCustomizationAppearance& Appearance);
};
