#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Customization/CustomizationProfile.h"
#include "Kismet/GameplayStatics.h"
#include <limits>
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCustomizationStorageTest,"FPS.Customization.Storage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCustomizationStorageTest::RunTest(const FString&)
{
 using S=FCustomizationStorage;
 TestNotEqual(TEXT("accounts separated"),S::SlotFor(TEXT("STEAM"),TEXT("A"),TEXT("Ranger")),S::SlotFor(TEXT("STEAM"),TEXT("B"),TEXT("Ranger")));
 TestNotEqual(TEXT("classes separated"),S::SlotFor(TEXT("STEAM"),TEXT("A"),TEXT("Ranger")),S::SlotFor(TEXT("STEAM"),TEXT("A"),TEXT("Medic")));
 TestNotEqual(TEXT("test and steam separated"),S::SlotFor(TEXT("LOCAL_TEST"),TEXT("A"),TEXT("Ranger")),S::SlotFor(TEXT("STEAM"),TEXT("A"),TEXT("Ranger")));
 FCustomizationAppearance D;TestTrue(TEXT("defaults valid"),D.Validate());D.Values[0]=std::numeric_limits<float>::quiet_NaN();TestFalse(TEXT("NaN rejected"),D.Validate());D=FCustomizationAppearance();D.Colors.RemoveAt(0);TestFalse(TEXT("missing part rejected"),D.Validate());D=FCustomizationAppearance();D.BodyType=TEXT("Unknown");TestFalse(TEXT("unknown body rejected"),D.Validate());
 D=FCustomizationAppearance();D.BodyType=TEXT("Fab");D.HairId=TEXT("Bald");D.SkinValue=.7f;D.EyeColor=FLinearColor(.1f,.4f,.7f);D.SkinColor=FLinearColor(.8f,.6f,.5f);
 for(int I=0;I<8;++I){D.Colors[I]=FLinearColor(.05f*I,.4f,.7f);D.Values[I]=.2f*I;D.Originals[I]=.1f*I;}
 FString Id=TEXT("AUTOMATION_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);FCustomizationAppearance Out;
 TestTrue(TEXT("missing distinct"),S::Load(TEXT("LOCAL_TEST"),Id,TEXT("Ranger"),Out)==ECustomizationLoadResult::Missing);
 TestTrue(TEXT("save"),S::Save(TEXT("LOCAL_TEST"),Id,TEXT("Ranger"),D));
 TestTrue(TEXT("load"),S::Load(TEXT("LOCAL_TEST"),Id,TEXT("Ranger"),Out)==ECustomizationLoadResult::Loaded);
 TestEqual(TEXT("body roundtrip"),Out.BodyType,D.BodyType);TestEqual(TEXT("hair roundtrip"),Out.HairId,D.HairId);TestEqual(TEXT("skin brightness"),Out.SkinValue,D.SkinValue);TestTrue(TEXT("skin color"),Out.SkinColor==D.SkinColor);TestTrue(TEXT("eye color"),Out.EyeColor==D.EyeColor);
 for(int I=0;I<8;++I){TestTrue(TEXT("color roundtrip"),Out.Colors[I]==D.Colors[I]);TestEqual(TEXT("value roundtrip"),Out.Values[I],D.Values[I]);TestEqual(TEXT("original roundtrip"),Out.Originals[I],D.Originals[I]);}
 auto* P=NewObject<UCustomizationProfile>();P->Service=TEXT("LOCAL_TEST");P->Account=TEXT("Other");P->Appearance=D;
 const FString Slot=S::SlotFor(TEXT("LOCAL_TEST"),Id,TEXT("Ranger"));UGameplayStatics::SaveGameToSlot(P,Slot,0);
 TestTrue(TEXT("wrong owner rejected"),S::Load(TEXT("LOCAL_TEST"),Id,TEXT("Ranger"),Out)==ECustomizationLoadResult::Invalid);
 TestFalse(TEXT("invalid slot not overwritten"),S::Save(TEXT("LOCAL_TEST"),Id,TEXT("Ranger"),D));
 // Delete only the GUID-named test slot created above; never user saves.
 UGameplayStatics::DeleteGameInSlot(Slot,0);return true;
}
#endif
