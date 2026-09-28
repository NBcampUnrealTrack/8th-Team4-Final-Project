#include "Customization/AppearanceCloudStore.h"
FAppearanceCloudStore::FAppearanceCloudStore(FString InAccount, TSharedPtr<IAppearanceCloudBackend> InBackend, FAppearanceLocalIO InLocal)
 : Account(MoveTemp(InAccount)), Backend(MoveTemp(InBackend)), Local(MoveTemp(InLocal)) {}
void FAppearanceCloudStore::Refresh(FName Class)
{
 if (!Local.IsCurrent() || AppearanceCloud::FileName(Account, Class).IsEmpty()) return;
 auto& State = States.FindOrAdd(Class);
 if (State.Busy) { State.Queued = true; return; }
 if (!Backend) { State.Status = EAppearanceSaveStatus::LocalOnly; return; }
 if (Backend->NeedsRestart()) { State.Status = EAppearanceSaveStatus::RestartRequired; return; }
 if (!Backend->IsEnabled()) { State.Status = EAppearanceSaveStatus::LocalOnly; return; }
 State.Busy = true; State.Queued = false; State.Status = EAppearanceSaveStatus::Pending;
 TWeakPtr<FAppearanceCloudStore> Weak = AsShared();
 Backend->Read(Account, AppearanceCloud::FileName(Account, Class), [Weak, Class](EAppearanceCloudRead Result, TArray<uint8> Bytes)
 {
  if (auto Self = Weak.Pin(); Self && Self->Local.IsCurrent()) Self->ReadComplete(Class, Result, MoveTemp(Bytes));
 });
}
void FAppearanceCloudStore::ReadComplete(FName Class, EAppearanceCloudRead Result, TArray<uint8> Bytes)
{
 if (Result == EAppearanceCloudRead::Disabled) { Finish(Class, EAppearanceSaveStatus::LocalOnly); return; }
 if (Result == EAppearanceCloudRead::RestartRequired) { Finish(Class, EAppearanceSaveStatus::RestartRequired); return; }
 if (Result == EAppearanceCloudRead::Failed) { Finish(Class, EAppearanceSaveStatus::CloudFailed); return; }
 FCustomizationAppearance Remote, Saved;
 if (Result == EAppearanceCloudRead::Found && !AppearanceCloud::Decode(Account, Class, Bytes, Remote))
 { Finish(Class, EAppearanceSaveStatus::CloudFailed); return; }
 const auto LocalResult = Local.Load(Class, Saved);
 if (LocalResult == ECustomizationLoadResult::Invalid) { Finish(Class, EAppearanceSaveStatus::LocalFailed); return; }
 const FString LocalHash = LocalResult == ECustomizationLoadResult::Loaded ? AppearanceCloud::Hash(Saved) : FString();
 const FString RemoteHash = Result == EAppearanceCloudRead::Found ? AppearanceCloud::Hash(Remote) : FString();
 const auto Decision = DecideAppearanceSync(Local.LoadBase(Class), LocalHash, RemoteHash);
 if (Decision == EAppearanceSyncDecision::None)
 {
  States[Class].Conflict.Reset();
  Finish(Class, Local.SaveBase(Class, LocalHash) ? EAppearanceSaveStatus::Synced : EAppearanceSaveStatus::LocalFailed);
 }
 else if (Decision == EAppearanceSyncDecision::Download)
 {
  if (!Local.Save(Class, Remote) || !Local.SaveBase(Class, RemoteHash)) { Finish(Class, EAppearanceSaveStatus::LocalFailed); return; }
  States[Class].Conflict.Reset();
  Finish(Class, EAppearanceSaveStatus::Synced);
 }
 else if (Decision == EAppearanceSyncDecision::Conflict)
 {
  // The existing local save remains untouched; keep the other copy on disk too.
  if (!Local.SaveConflict(Class, Bytes)) { Finish(Class, EAppearanceSaveStatus::LocalFailed); return; }
  States[Class].Conflict = Remote;
  Finish(Class, EAppearanceSaveStatus::Conflict);
 }
 else
 {
  TArray<uint8> Upload;
  if (!AppearanceCloud::Encode(Account, Class, Saved, Upload)) { Finish(Class, EAppearanceSaveStatus::LocalFailed); return; }
  TWeakPtr<FAppearanceCloudStore> Weak = AsShared();
  Backend->Write(Account, AppearanceCloud::FileName(Account, Class), MoveTemp(Upload), [Weak, Class, LocalHash](bool Success)
  {
   auto Self = Weak.Pin(); if (!Self || !Self->Local.IsCurrent()) return;
   if (Self->Backend->NeedsRestart()) { Self->Finish(Class, EAppearanceSaveStatus::RestartRequired); return; }
   if (!Self->Backend->IsEnabled()) { Self->Finish(Class, EAppearanceSaveStatus::LocalOnly); return; }
   if (!Success) { Self->Finish(Class, EAppearanceSaveStatus::CloudFailed); return; }
   // Do not mark a newer save as uploaded: this hash belongs to this exact request.
   if (!Self->Local.SaveBase(Class, LocalHash)) { Self->Finish(Class, EAppearanceSaveStatus::LocalFailed); return; }
   Self->States[Class].Conflict.Reset();
   Self->Finish(Class, EAppearanceSaveStatus::Synced);
  });
 }
}
void FAppearanceCloudStore::Finish(FName Class, EAppearanceSaveStatus NewStatus)
{
 auto& State = States[Class]; State.Status = NewStatus; State.Busy = false;
 const bool Again = State.Queued; State.Queued = false;
 if (Again && NewStatus == EAppearanceSaveStatus::Synced) Refresh(Class);
}
void FAppearanceCloudStore::Resolve(FName Class, bool UseLocal)
{
 auto* State = States.Find(Class);
 if (!Local.IsCurrent() || !State || State->Busy || !State->Conflict.IsSet()) return;
 const auto Remote = State->Conflict.GetValue();
 if (!UseLocal && !Local.Save(Class, Remote)) { State->Status = EAppearanceSaveStatus::LocalFailed; return; }
 // Treat the explicitly accepted remote revision as the common ancestor, then
 // re-read before writing. A subsequently changed remote copy conflicts again.
 if (!Local.SaveBase(Class, AppearanceCloud::Hash(Remote))) { State->Status = EAppearanceSaveStatus::LocalFailed; return; }
 State->Conflict.Reset(); Refresh(Class);
}
EAppearanceSaveStatus FAppearanceCloudStore::Status(FName Class) const
{
 const auto* State = States.Find(Class);
 return State ? State->Status : EAppearanceSaveStatus::LocalOnly;
}
