#pragma once
#include "CoreMinimal.h"
inline bool IsCustomizationBodyForClass(const FString& C,const FString& B)
{
 if(C==TEXT("Ranger"))return B==TEXT("Seo")||B==TEXT("Custom")||B==TEXT("Fab");
 if(C==TEXT("Trooper"))return B==TEXT("MHC_Frey")||B==TEXT("MHC_Gavin")||B==TEXT("Mason");
 if(C==TEXT("Vanguard"))return B==TEXT("MHC_Crowd80")||B==TEXT("Dean")||B==TEXT("MHC_Shrank");
 return false;
}
