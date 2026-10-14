#pragma once
#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Customization/CustomizationProfile.h"
#include "Customization/AppearanceCloudStore.h"
#include "AppearanceLocalPlayerSubsystem.generated.h"

UCLASS()
class FPS_API UAppearanceLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()
public:
    void RefreshIdentity();
    ECustomizationLoadResult LoadSavedAppearance(FName ClassId, FCustomizationAppearance& Out);
    bool SaveAppearance(FName ClassId, const FCustomizationAppearance& Data);
    void RefreshCloud();
    void ResolveConflict(FName ClassId, bool UseLocal);
    EAppearanceSaveStatus GetSaveStatus(FName ClassId) const;
    FString GetStatusText(FName ClassId) const;
    virtual void Deinitialize() override;
    const FString& GetService() const { return Service; }
    const FString& GetAccount() const { return Account; }
private:
    FString Service;
    FString Account;
    uint32 IdentityGeneration = 0;
    TSharedPtr<IAppearanceCloudBackend> Backend;
    TSharedPtr<FAppearanceCloudStore> Cloud;
    TSet<FName> FailedLocalSaves;
};
