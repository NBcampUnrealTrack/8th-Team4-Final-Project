#include "Misc/AutomationTest.h"
#include "Customization/CustomizationProfile.h"
#include "Customization/CustomizationStudioActor.h"
#include "Customization/CustomizationStudioController.h"
#include "MuCO/CustomizableObject.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCustomizationMissingBindingTest,"FPS.Customization.Classes.MissingBinding",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCustomizationMissingBindingTest::RunTest(const FString&)
{
 auto* Original=LoadObject<UCustomizationStudioConfig>(nullptr,TEXT("/Game/FPS/Customizable/Character/Preview/DA_StudioTrooper"));if(!TestNotNull(TEXT("Trooper config"),Original))return false;
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);auto* S=W->SpawnActor<ACustomizationStudioActor>();auto* C=W->SpawnActor<ACustomizationStudioController>();C->Studio=S;
 auto* Config=DuplicateObject<UCustomizationStudioConfig>(Original,S);S->Config=Config;C->ActiveClass=TEXT("Trooper");
 Config->Grooms.RemoveAll([](const FStudioGroomChoice& H){return H.Component==TEXT("Hair")&&H.Id==TEXT("Mason")&&H.BodyType==TEXT("MHC_Gavin");});
 C->Draft.BodyType=TEXT("MHC_Frey");C->Draft.HairId=TEXT("Mason");C->SelectBody(TEXT("MHC_Gavin"));
 TestEqual(TEXT("missing destination binding retains selection"),C->Draft.HairId,FName(TEXT("Mason")));TestFalse(TEXT("invalid selection cannot save"),C->CanSave());
 const FString Account=TEXT("MissingBinding_")+FGuid::NewGuid().ToString();FCustomizationAppearance D;D.BodyType=TEXT("MHC_Gavin");D.HairId=TEXT("Mason");
 TestTrue(TEXT("test-only saved missing hair"),FCustomizationStorage::Save(TEXT("LOCAL_TEST"),Account,TEXT("Trooper"),D));
 C->SetAccountIdentity(TEXT("LOCAL_TEST"),Account);TestEqual(TEXT("load retains unavailable selection"),C->Draft.HairId,FName(TEXT("Mason")));TestFalse(TEXT("loaded missing hair cannot save"),C->CanSave());
 UGameplayStatics::DeleteGameInSlot(FCustomizationStorage::SlotFor(TEXT("LOCAL_TEST"),Account,TEXT("Trooper")),0);
 W->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCustomizationClassFilesTest,"FPS.Customization.Classes.SeparateFiles",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCustomizationClassFilesTest::RunTest(const FString&)
{
 const FString Account=TEXT("ClassIsolation_")+FGuid::NewGuid().ToString();
 const TArray<FString> Classes={TEXT("Ranger"),TEXT("Trooper"),TEXT("Vanguard")};
 const TArray<FString> Bodies={TEXT("Seo"),TEXT("MHC_Frey"),TEXT("Dean")};
 for(int32 I=0;I<3;++I){FCustomizationAppearance D;D.BodyType=Bodies[I];D.HairId=TEXT("Bald");D.SkinValue=.4f+.2f*I;TestTrue(TEXT("class save"),FCustomizationStorage::Save(TEXT("LOCAL_TEST"),Account,Classes[I],D));}
 for(int32 I=0;I<3;++I){FCustomizationAppearance D;TestEqual(TEXT("class load"),FCustomizationStorage::Load(TEXT("LOCAL_TEST"),Account,Classes[I],D),ECustomizationLoadResult::Loaded);TestEqual(TEXT("body isolated"),D.BodyType,Bodies[I]);TestEqual(TEXT("color isolated"),D.SkinValue,.4f+.2f*I);}
 const FString Slot=FCustomizationStorage::SlotFor(TEXT("LOCAL_TEST"),Account,TEXT("Trooper"));
 auto* Invalid=NewObject<UCustomizationProfile>();Invalid->Service=TEXT("LOCAL_TEST");Invalid->Account=Account;Invalid->CharacterClass=TEXT("Trooper");Invalid->Appearance.BodyType=TEXT("Dean");
 TestTrue(TEXT("write test-only cross-class payload"),UGameplayStatics::SaveGameToSlot(Invalid,Slot,0));
 FCustomizationAppearance D;TestEqual(TEXT("reject cross-class file"),FCustomizationStorage::Load(TEXT("LOCAL_TEST"),Account,TEXT("Trooper"),D),ECustomizationLoadResult::Invalid);
 D.BodyType=TEXT("MHC_Frey");TestFalse(TEXT("do not overwrite invalid file"),FCustomizationStorage::Save(TEXT("LOCAL_TEST"),Account,TEXT("Trooper"),D));
 for(const auto& C:Classes){const FString Own=FCustomizationStorage::SlotFor(TEXT("LOCAL_TEST"),Account,C);UGameplayStatics::DeleteGameInSlot(Own,0);UGameplayStatics::DeleteGameInSlot(Own+TEXT("_backup"),0);}
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCustomizationClassSwitchTest,"FPS.Customization.Classes.SwitchDrafts",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCustomizationClassSwitchTest::RunTest(const FString&)
{
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);
 auto* S=W->SpawnActor<ACustomizationStudioActor>();auto* C=W->SpawnActor<ACustomizationStudioController>();C->Studio=S;
 for(const TCHAR* Name:{TEXT("Ranger"),TEXT("Trooper"),TEXT("Vanguard")})
 {
  auto* Config=NewObject<UCustomizationStudioConfig>(S);Config->ClassId=Name;
  Config->CustomizableObject=LoadObject<UCustomizableObject>(nullptr,*(FString(TEXT("/Game/FPS/Customizable/CO_"))+Name));
  Config->DefaultBodyType=FString(Name)==TEXT("Ranger")?TEXT("Seo"):FString(Name)==TEXT("Trooper")?TEXT("MHC_Frey"):TEXT("Dean");Config->BodyTypes={Config->DefaultBodyType};Config->DefaultHairId=TEXT("Bald");S->ClassConfigs.Add(Name,Config);
 }
 S->Config=S->ClassConfigs.FindChecked(TEXT("Ranger"));
 TestTrue(TEXT("Select Trooper"),C->SelectClass(TEXT("Trooper")));
 C->Draft.SkinValue=.77f;C->Changed();
 TestTrue(TEXT("Select Vanguard"),C->SelectClass(TEXT("Vanguard")));
 TestTrue(TEXT("Return Trooper"),C->SelectClass(TEXT("Trooper")));
 TestEqual(TEXT("draft survives"),C->Draft.SkinValue,.77f);TestTrue(TEXT("dirty survives"),C->bDirty);
 TestTrue(TEXT("same class no-op"),C->SelectClass(TEXT("Trooper")));
 TestFalse(TEXT("unknown denied"),C->SelectClass(TEXT("Other")));
 C->BeginColorEdit(8);TestFalse(TEXT("modal blocks switch"),C->SelectClass(TEXT("Vanguard")));C->EndColorEdit(false);
 TestFalse(TEXT("cross-class body invalid"),S->IsBodyValid(TEXT("Dean")));
 C->SetAccountIdentity(TEXT("LOCAL_TEST"),FGuid::NewGuid().ToString());
 TestFalse(TEXT("new account clean"),C->bDirty);TestEqual(TEXT("new account default"),C->Draft.SkinValue,1.f);
 C->SelectClass(TEXT("Vanguard"));C->SelectClass(TEXT("Trooper"));TestEqual(TEXT("old cache cleared"),C->Draft.SkinValue,1.f);
 W->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCustomizationClassRulesTest,"FPS.Customization.Classes.StorageRules",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCustomizationClassRulesTest::RunTest(const FString&)
{
 FCustomizationAppearance D;
 TestTrue(TEXT("legacy Ranger"),D.ValidateForClass(TEXT("Ranger")));
 D.BodyType=TEXT("MHC_Frey");
 TestTrue(TEXT("Frey Trooper"),D.ValidateForClass(TEXT("Trooper")));
 TestFalse(TEXT("cross class"),D.ValidateForClass(TEXT("Vanguard")));
 TestFalse(TEXT("unknown class"),D.ValidateForClass(TEXT("Other")));
 D.BodyType=TEXT("Dean");
 TestTrue(TEXT("Dean Vanguard"),D.ValidateForClass(TEXT("Vanguard")));
 TestFalse(TEXT("Dean Ranger"),D.ValidateForClass(TEXT("Ranger")));
 D.BodyType=TEXT("Unknown");TestFalse(TEXT("unknown body"),D.Validate());
 TestNotEqual(TEXT("class storage separated"),FCustomizationStorage::SlotFor(TEXT("LOCAL_TEST"),TEXT("Classes"),TEXT("Ranger")),FCustomizationStorage::SlotFor(TEXT("LOCAL_TEST"),TEXT("Classes"),TEXT("Trooper")));
 return true;
}
