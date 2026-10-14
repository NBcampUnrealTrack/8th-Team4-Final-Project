#pragma once
#include "CoreMinimal.h"
#include "Customization/AppearanceCloudPolicy.h"

enum class EAppearanceSaveStatus { LocalOnly, Pending, Synced, Conflict, CloudFailed, LocalFailed, RestartRequired };
enum class EAppearanceCloudRead { Found, Missing, Failed, Disabled, RestartRequired };
class FPS_API IAppearanceCloudBackend
{
public:
 virtual ~IAppearanceCloudBackend() = default;
 virtual bool IsEnabled() const { return true; }
 virtual bool NeedsRestart() const { return false; }
 virtual void Read(const FString& Account, const FString& File, TFunction<void(EAppearanceCloudRead, TArray<uint8>)> Done) = 0;
 virtual void Write(const FString& Account, const FString& File, TArray<uint8> Bytes, TFunction<void(bool)> Done) = 0;
};
FPS_API TSharedPtr<IAppearanceCloudBackend> CreateSteamAppearanceBackend();

// Injected local I/O keeps conflict handling independently testable without Steam.
struct FAppearanceLocalIO
{
 TFunction<bool()> IsCurrent;
 TFunction<ECustomizationLoadResult(FName, FCustomizationAppearance&)> Load;
 TFunction<bool(FName, const FCustomizationAppearance&)> Save;
 TFunction<FString(FName)> LoadBase;
 TFunction<bool(FName, const FString&)> SaveBase;
 TFunction<bool(FName, const TArray<uint8>&)> SaveConflict;
};

class FPS_API FAppearanceCloudStore : public TSharedFromThis<FAppearanceCloudStore>
{
public:
 FAppearanceCloudStore(FString InAccount, TSharedPtr<IAppearanceCloudBackend> InBackend, FAppearanceLocalIO InLocal);
 void Refresh(FName Class);
 void Resolve(FName Class, bool UseLocal);
 EAppearanceSaveStatus Status(FName Class) const;
private:
 struct FState
 {
  EAppearanceSaveStatus Status = EAppearanceSaveStatus::LocalOnly;
  bool Busy = false;
  bool Queued = false;
  TOptional<FCustomizationAppearance> Conflict;
 };
 void ReadComplete(FName Class, EAppearanceCloudRead Result, TArray<uint8> Bytes);
 void Finish(FName Class, EAppearanceSaveStatus Status);
 FString Account;
 TSharedPtr<IAppearanceCloudBackend> Backend;
 FAppearanceLocalIO Local;
 TMap<FName, FState> States;
};
