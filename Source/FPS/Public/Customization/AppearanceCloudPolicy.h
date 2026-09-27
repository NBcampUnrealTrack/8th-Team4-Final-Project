#pragma once
#include "CoreMinimal.h"
#include "Customization/CustomizationProfile.h"

enum class EAppearanceSyncDecision { None, Download, Upload, Conflict };
FPS_API EAppearanceSyncDecision DecideAppearanceSync(const FString& Base, const FString& Local, const FString& Remote);
namespace AppearanceCloud
{
 constexpr int32 MaxFileBytes = 64 * 1024;
 FPS_API FString FileName(const FString& Account, FName ClassId);
 FPS_API FString Hash(const FCustomizationAppearance& Appearance);
 FPS_API bool Encode(const FString& Account, FName ClassId, const FCustomizationAppearance& Appearance, TArray<uint8>& Out);
 FPS_API bool Decode(const FString& Account, FName ClassId, const TArray<uint8>& Bytes, FCustomizationAppearance& Out);
}
