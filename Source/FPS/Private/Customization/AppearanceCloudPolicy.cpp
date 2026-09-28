#include "Customization/AppearanceCloudPolicy.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/SecureHash.h"

EAppearanceSyncDecision DecideAppearanceSync(const FString& Base, const FString& Local, const FString& Remote)
{
 if (Local == Remote) return EAppearanceSyncDecision::None;
 if (Local.IsEmpty()) return EAppearanceSyncDecision::Download;
 if (Remote.IsEmpty()) return EAppearanceSyncDecision::Upload;
 if (!Base.IsEmpty() && Local == Base) return EAppearanceSyncDecision::Download;
 if (!Base.IsEmpty() && Remote == Base) return EAppearanceSyncDecision::Upload;
 return EAppearanceSyncDecision::Conflict;
}
namespace
{
uint8 ClassCode(FName Class)
{
 return Class == TEXT("Ranger") ? 1 : Class == TEXT("Trooper") ? 2 : Class == TEXT("Vanguard") ? 3 : 0;
}
bool ValidAccount(const FString& Account)
{
 if (Account.Len() != 17) return false;
 for (TCHAR C : Account) if (C < TEXT('0') || C > TEXT('9')) return false;
 return true;
}
FString BytesHash(const TArray<uint8>& Bytes)
{
 FMD5 Md5; Md5.Update(Bytes.GetData(), Bytes.Num()); uint8 Digest[16]; Md5.Final(Digest);
 return BytesToHex(Digest, 16);
}
}
FString AppearanceCloud::FileName(const FString& Account, FName Class)
{
 if (!ValidAccount(Account) || !ClassCode(Class)) return {};
 return TEXT("fps_team4_appearance_v1_") + Account + TEXT("_") + Class.ToString() + TEXT(".sav");
}
FString AppearanceCloud::Hash(const FCustomizationAppearance& Appearance)
{
 if (!Appearance.Validate()) return {};
 FCustomizationAppearance Copy = Appearance;
 TArray<uint8> Bytes; FMemoryWriter Writer(Bytes); bool Success = false;
 Copy.NetSerialize(Writer, nullptr, Success);
 return Success ? BytesHash(Bytes) : FString();
}
bool AppearanceCloud::Encode(const FString& Account, FName Class, const FCustomizationAppearance& Appearance, TArray<uint8>& Out)
{
 Out.Reset();
 if (FileName(Account, Class).IsEmpty() || !Appearance.ValidateForClass(Class.ToString())) return false;
 FMemoryWriter Writer(Out);
 uint32 Magic = 0x34535046, Schema = 1; uint8 Code = ClassCode(Class);
 Writer << Magic << Schema;
 for (TCHAR C : Account) { uint8 Digit = uint8(C); Writer << Digit; }
 Writer << Code;
 FCustomizationAppearance Copy = Appearance; bool Success = false;
 Copy.NetSerialize(Writer, nullptr, Success);
 if (!Success) { Out.Reset(); return false; }
 const FString Digest = BytesHash(Out);
 for (TCHAR C : Digest) { uint8 Digit = uint8(C); Writer << Digit; }
 return !Writer.IsError() && Out.Num() <= MaxFileBytes;
}
bool AppearanceCloud::Decode(const FString& Account, FName Class, const TArray<uint8>& Bytes, FCustomizationAppearance& Out)
{
 if (FileName(Account, Class).IsEmpty() || Bytes.Num() < 64 || Bytes.Num() > MaxFileBytes) return false;
 TArray<uint8> Payload(Bytes.GetData(), Bytes.Num() - 32);
 const FString ExpectedHash = BytesHash(Payload);
 for (int32 I = 0; I < 32; ++I) if (Bytes[Payload.Num() + I] != uint8(ExpectedHash[I])) return false;
 FMemoryReader Reader(Payload);
 uint32 Magic = 0, Schema = 0; Reader << Magic << Schema;
 if (Magic != 0x34535046 || Schema != 1) return false;
 for (TCHAR C : Account) { uint8 Digit = 0; Reader << Digit; if (Digit != uint8(C)) return false; }
 uint8 Code = 0; Reader << Code; if (Code != ClassCode(Class)) return false;
 FCustomizationAppearance Candidate; bool Success = false;
 Candidate.NetSerialize(Reader, nullptr, Success);
 if (!Success || Reader.IsError() || !Reader.AtEnd() || !Candidate.ValidateForClass(Class.ToString())) return false;
 Out = MoveTemp(Candidate);
 return true;
}
