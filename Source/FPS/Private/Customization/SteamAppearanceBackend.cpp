#include "Customization/AppearanceCloudStore.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemTypes.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineUserCloudInterface.h"
#include "Containers/Ticker.h"
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace
{
class FSteamAppearanceBackend final : public IAppearanceCloudBackend
{
 struct FJob
 {
  FString Account, File;
  TArray<uint8> Bytes;
  TFunction<void(EAppearanceCloudRead, TArray<uint8>)> ReadDone;
  TFunction<void(bool)> WriteDone;
  bool Writing = false;
  bool TimedOut = false;
  double Started = 0;
 };
public:
 virtual bool IsEnabled() const override
 {
  auto* Storage = SteamRemoteStorage();
  return Storage && Storage->IsCloudEnabledForAccount() && Storage->IsCloudEnabledForApp();
 }
 virtual bool NeedsRestart() const override { return Active && Active->TimedOut; }
 explicit FSteamAppearanceBackend(IOnlineUserCloudPtr InCloud) : Cloud(MoveTemp(InCloud))
 {
  Enumerated = Cloud->AddOnEnumerateUserFilesCompleteDelegate_Handle(FOnEnumerateUserFilesCompleteDelegate::CreateRaw(this, &FSteamAppearanceBackend::OnEnumerated));
  ReadHandle = Cloud->AddOnReadUserFileCompleteDelegate_Handle(FOnReadUserFileCompleteDelegate::CreateRaw(this, &FSteamAppearanceBackend::OnRead));
  WriteHandle = Cloud->AddOnWriteUserFileCompleteDelegate_Handle(FOnWriteUserFileCompleteDelegate::CreateRaw(this, &FSteamAppearanceBackend::OnWrite));
  Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FSteamAppearanceBackend::Tick), 1.f);
 }
 virtual ~FSteamAppearanceBackend() override
 {
  FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
  Cloud->ClearOnEnumerateUserFilesCompleteDelegate_Handle(Enumerated);
  Cloud->ClearOnReadUserFileCompleteDelegate_Handle(ReadHandle);
  Cloud->ClearOnWriteUserFileCompleteDelegate_Handle(WriteHandle);
 }
 virtual void Read(const FString& Account, const FString& File, TFunction<void(EAppearanceCloudRead, TArray<uint8>)> Done) override
 {
  FJob Job; Job.Account = Account; Job.File = File; Job.ReadDone = MoveTemp(Done); Enqueue(MoveTemp(Job));
 }
 virtual void Write(const FString& Account, const FString& File, TArray<uint8> Bytes, TFunction<void(bool)> Done) override
 {
  FJob Job; Job.Account = Account; Job.File = File; Job.Bytes = MoveTemp(Bytes); Job.WriteDone = MoveTemp(Done); Job.Writing = true; Enqueue(MoveTemp(Job));
 }
private:
 void Reject(FJob& Job, EAppearanceCloudRead Reason = EAppearanceCloudRead::Failed)
 {
  if (Job.ReadDone) { auto Done = MoveTemp(Job.ReadDone); Done(Reason, {}); }
  if (Job.WriteDone) { auto Done = MoveTemp(Job.WriteDone); Done(false); }
 }
 void Enqueue(FJob Job)
 {
  if (NeedsRestart()) { Reject(Job, EAppearanceCloudRead::RestartRequired); return; }
  if (!IsEnabled()) { Reject(Job, EAppearanceCloudRead::Disabled); return; }
  const bool KnownFile = Job.File == AppearanceCloud::FileName(Job.Account, TEXT("Ranger"))
   || Job.File == AppearanceCloud::FileName(Job.Account, TEXT("Trooper")) || Job.File == AppearanceCloud::FileName(Job.Account, TEXT("Vanguard"));
  if (!KnownFile || Job.File.IsEmpty() || Job.Bytes.Num() > AppearanceCloud::MaxFileBytes || Queue.Num() >= 6)
  { Reject(Job); return; }
  // The platform delegates contain no request ID. Never reuse an ambiguous
  // timed-out operation until its actual completion arrives (or process restart).
  if (Active && Active->Account == Job.Account && Active->File == Job.File) { Reject(Job); return; }
  Job.Started = FPlatformTime::Seconds(); Queue.Add(MoveTemp(Job)); StartNext();
 }
 void StartNext()
 {
  if (Active || Queue.IsEmpty()) return;
  Active = MakeUnique<FJob>(MoveTemp(Queue[0])); Queue.RemoveAt(0);
  if (!IsEnabled()) { Complete(false); return; }
  IOnlineSubsystem* Steam = IOnlineSubsystem::Get(TEXT("STEAM"));
  const IOnlineIdentityPtr Identity = Steam ? Steam->GetIdentityInterface() : nullptr;
  User = Identity ? Identity->GetUniquePlayerId(0) : nullptr;
  if (!User || !User->IsValid() || User->ToString() != Active->Account) { Complete(false); return; }
  if (Active->Writing)
  {
   if (!Cloud->WriteUserFile(*User, Active->File, Active->Bytes, false)) Complete(false);
  }
  else Cloud->EnumerateUserFiles(*User);
 }
 bool Matches(const FUniqueNetId& Id, const FString* File = nullptr) const
 { return Active && Id.ToString() == Active->Account && (!File || *File == Active->File); }
 void OnEnumerated(bool Success, const FUniqueNetId& Id)
 {
  if (!Matches(Id) || Active->Writing) return;
  if (!Success || Active->TimedOut || !IsEnabled()) { Complete(false); return; }
  TArray<FCloudFileHeader> Files; Cloud->GetUserFileList(Id, Files);
  const auto* File = Files.FindByPredicate([this](const FCloudFileHeader& Row){ return Row.FileName == Active->File; });
  if (!File) { Complete(true, EAppearanceCloudRead::Missing); return; }
  if (File->FileSize <= 0 || File->FileSize > AppearanceCloud::MaxFileBytes) { Complete(false); return; }
  if (!Cloud->ReadUserFile(Id, Active->File)) Complete(false);
 }
 void OnRead(bool Success, const FUniqueNetId& Id, const FString& File)
 {
  if (!Matches(Id, &File) || Active->Writing) return;
  TArray<uint8> Bytes;
  if (!Success || Active->TimedOut || !Cloud->GetFileContents(Id, File, Bytes) || Bytes.Num() > AppearanceCloud::MaxFileBytes)
  { Complete(false); return; }
  Complete(true, EAppearanceCloudRead::Found, MoveTemp(Bytes));
 }
 void OnWrite(bool Success, const FUniqueNetId& Id, const FString& File)
 { if (Matches(Id, &File) && Active->Writing) Complete(Success); }
 void Complete(bool Success, EAppearanceCloudRead Result = EAppearanceCloudRead::Failed, TArray<uint8> Bytes = {})
 {
  auto Job = MoveTemp(Active); User.Reset();
  if (!Job) return;
  const bool Enabled = IsEnabled();
  if (Job->ReadDone) Job->ReadDone(!Enabled ? EAppearanceCloudRead::Disabled : (Success ? Result : EAppearanceCloudRead::Failed), MoveTemp(Bytes));
  if (Job->WriteDone) Job->WriteDone(Success && Enabled);
  StartNext();
 }
 bool Tick(float)
 {
  const double Now = FPlatformTime::Seconds();
  if (Active && !Active->TimedOut && Now - Active->Started >= 15)
  {
   Active->TimedOut = true; Reject(*Active, EAppearanceCloudRead::RestartRequired);
   auto Blocked = MoveTemp(Queue); Queue.Reset();
   for (auto& Job : Blocked) Reject(Job, EAppearanceCloudRead::RestartRequired);
  }
  for (int32 I = Queue.Num() - 1; I >= 0; --I)
   if (Now - Queue[I].Started >= 15) { auto Expired = MoveTemp(Queue[I]); Queue.RemoveAt(I); Reject(Expired); }
  return true;
 }
 IOnlineUserCloudPtr Cloud;
 FUniqueNetIdPtr User;
 TArray<FJob> Queue;
 TUniquePtr<FJob> Active;
 FDelegateHandle Enumerated, ReadHandle, WriteHandle;
 FTSTicker::FDelegateHandle Ticker;
};
}
TSharedPtr<IAppearanceCloudBackend> CreateSteamAppearanceBackend()
{
 IOnlineSubsystem* Steam = IOnlineSubsystem::Get(TEXT("STEAM"));
 const auto Cloud = Steam ? Steam->GetUserCloudInterface() : nullptr;
 if (!Cloud) return nullptr;
 return MakeShared<FSteamAppearanceBackend>(Cloud);
}
