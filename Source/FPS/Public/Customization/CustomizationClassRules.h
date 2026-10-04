#pragma once
#include "CoreMinimal.h"
// Fixed identities used by the studio, network presentation and default spawn.
// Legacy body IDs below remain readable so old saves can be migrated without losing colors.
inline FString FixedCustomizationBodyForClass(const FString& C)
{
 if(C==TEXT("Ranger"))return TEXT("Seo");
 if(C==TEXT("Trooper"))return TEXT("MHC_Frey");
 if(C==TEXT("Vanguard"))return TEXT("MHC_Crowd80");
 return {};
}
inline FName FixedCustomizationHairForClass(const FString& C)
{
 if(C==TEXT("Ranger"))return TEXT("Default");
 return FName(*FixedCustomizationBodyForClass(C));
}
inline bool IsCustomizationBodyForClass(const FString& C,const FString& B)
{
 if(C==TEXT("Ranger"))return B==TEXT("Seo")||B==TEXT("Custom")||B==TEXT("Fab");
 if(C==TEXT("Trooper"))return B==TEXT("MHC_Frey")||B==TEXT("MHC_Gavin")||B==TEXT("Mason");
 if(C==TEXT("Vanguard"))return B==TEXT("MHC_Crowd80")||B==TEXT("Dean")||B==TEXT("MHC_Shrank");
 return false;
}

// IDs exported from the shipped class groom catalogs. No visual assets are
// loaded to validate network or cloud input on a dedicated server.
inline bool IsCustomizationHairForBody(const FString& ClassId, const FString& Body, FName Hair)
{
 if (!IsCustomizationBodyForClass(ClassId, Body)) return false;
 if (Hair == TEXT("Bald")) return true;
 if (ClassId == TEXT("Trooper"))
  return Hair == TEXT("MHC_Frey") || Hair == TEXT("MHC_Gavin") || Hair == TEXT("Mason");
 if (ClassId == TEXT("Vanguard"))
  return Hair == TEXT("MHC_Crowd80") || Hair == TEXT("Dean") || Hair == TEXT("MHC_Shrank");
 if (Hair == TEXT("Default")) return true;
 if (Body == TEXT("Seo")) return Hair == TEXT("GB_Seo_Ponytail_binding") || Hair == TEXT("GB_Binding_Mara_SEo");
 if (Body == TEXT("Custom")) return Hair == TEXT("GB_SeoHairBinding_Custom") || Hair == TEXT("GB_Ponytail_Custom");
 return Hair == TEXT("GB_SeoHairBinding_Mara") || Hair == TEXT("GB_PonytailBinding_Mara");
}
