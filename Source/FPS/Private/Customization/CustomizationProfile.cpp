#include "Customization/CustomizationProfile.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"

FCustomizationAppearance::FCustomizationAppearance(){Colors.Init(FLinearColor::White,8);Values.Init(1.f,8);Originals.Init(1.f,8);}
const TArray<FString>& FCustomizationAppearance::Parts(){static TArray<FString> P={TEXT("Top"),TEXT("Bottom"),TEXT("Vest"),TEXT("Attachment"),TEXT("Belt"),TEXT("Gloves"),TEXT("Boots"),TEXT("Scarf")};return P;}
bool FCustomizationAppearance::Validate() const
{
 auto Range=[](float V,float Min,float Max){return FMath::IsFinite(V)&&V>=Min&&V<=Max;};
 auto Color=[&](FLinearColor C){return Range(C.R,0,1)&&Range(C.G,0,1)&&Range(C.B,0,1)&&Range(C.A,0,1);};
 if(BodyType!=TEXT("Seo")&&BodyType!=TEXT("Custom")&&BodyType!=TEXT("Fab"))return false;
 if(HairId.IsNone()||HairId.ToString().Len()>128||!Color(HairColor)||!Color(EyeColor)||!Color(SkinColor)||!Range(SkinValue,.15f,1.35f))return false;
 if(Colors.Num()!=8||Values.Num()!=8||Originals.Num()!=8)return false;
 for(int32 I=0;I<8;++I)if(!Color(Colors[I])||!Range(Values[I],0,2)||!Range(Originals[I],0,1))return false;
 return true;
}
FString FCustomizationStorage::SlotFor(const FString& S,const FString& A,const FString& C)
{
 const FString Key=FString::Printf(TEXT("%d:%s%d:%s%d:%s"),S.Len(),*S,A.Len(),*A,C.Len(),*C);
 return TEXT("Customization_v1_")+FMD5::HashAnsiString(*Key);
}
bool FCustomizationStorage::Matches(const UCustomizationProfile* P,const FString& S,const FString& A,const FString& C)
{return P&&P->Version==1&&P->Service==S&&P->Account==A&&P->CharacterClass==C&&P->Appearance.Validate();}
ECustomizationLoadResult FCustomizationStorage::Load(const FString& S,const FString& A,const FString& C,FCustomizationAppearance& Out)
{
 if(S.IsEmpty()||A.IsEmpty()||C.IsEmpty())return ECustomizationLoadResult::Invalid;
 const FString Slot=SlotFor(S,A,C);if(!UGameplayStatics::DoesSaveGameExist(Slot,0))return ECustomizationLoadResult::Missing;
 auto* P=Cast<UCustomizationProfile>(UGameplayStatics::LoadGameFromSlot(Slot,0));
 if(!Matches(P,S,A,C))return ECustomizationLoadResult::Invalid;
 Out=P->Appearance;return ECustomizationLoadResult::Loaded;
}
bool FCustomizationStorage::Save(const FString& S,const FString& A,const FString& C,const FCustomizationAppearance& Data)
{
 if(S.IsEmpty()||A.IsEmpty()||C.IsEmpty()||!Data.Validate())return false;
 const FString Slot=SlotFor(S,A,C);
 if(UGameplayStatics::DoesSaveGameExist(Slot,0)){
  auto* Previous=Cast<UCustomizationProfile>(UGameplayStatics::LoadGameFromSlot(Slot,0));
  if(!Matches(Previous,S,A,C))return false;
  if(!UGameplayStatics::SaveGameToSlot(Previous,Slot+TEXT("_backup"),0))return false;
 }
 auto* P=NewObject<UCustomizationProfile>();P->Service=S;P->Account=A;P->CharacterClass=C;P->Appearance=Data;
 return UGameplayStatics::SaveGameToSlot(P,Slot,0);
}
