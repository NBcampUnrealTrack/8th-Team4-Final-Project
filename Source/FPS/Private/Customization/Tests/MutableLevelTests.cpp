#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Customization/CustomizationPreviewState.h"
#include "Customization/CustomizationAppearanceControls.h"
#include "Customization/CustomizationStudioActor.h"
#include "Customization/CustomizationStudioController.h"
#include "Engine/World.h"
#include "GroomAsset.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMutablePreviewReadyTest,
    "FPS.Customization.MutableLevel.Readiness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMutablePreviewReadyTest::RunTest(const FString&)
{
    // A stale, failed or pending preview must never authorize saving the draft.
    TestFalse(TEXT("No generated appearance"), IsPreviewRevisionReady(0, 0, false));
    TestFalse(TEXT("First request pending"), IsPreviewRevisionReady(1, 0, true));
    TestFalse(TEXT("Latest request failed or stale"), IsPreviewRevisionReady(2, 1, false));
    TestFalse(TEXT("Queued work blocks save"), IsPreviewRevisionReady(2, 2, true));
    TestTrue(TEXT("Latest preview is visible"), IsPreviewRevisionReady(2, 2, false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMutableBrightnessTest,
    "FPS.Customization.MutableLevel.Brightness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMutableBrightnessTest::RunTest(const FString&)
{
    FCustomizationAppearance Draft;
    TestEqual(TEXT("Clothing midpoint"), MutableBrightness(Draft,0), .5f);
    TestEqual(TEXT("Eyes have no brightness"), MutableBrightness(Draft,9), 0.f);
    TestEqual(TEXT("Invalid selection is safe"), MutableBrightness(Draft,-1), 0.f);
    Draft.Values.Reset();
    TestEqual(TEXT("Missing values are safe"), MutableBrightness(Draft,0), 0.f);
    Draft.SkinValue=.75f;
    TestTrue(TEXT("Skin normalization"), FMath::IsNearlyEqual(MutableBrightness(Draft,8),.5f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMutableHairNumberTest,
    "FPS.Customization.MutableLevel.HairNumbers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMutableHairNumberTest::RunTest(const FString&)
{
    auto* Config=LoadObject<UCustomizationStudioConfig>(nullptr,TEXT("/Game/FPS/Customizable/MutableLevel/DA_MutableLevelStudio.DA_MutableLevelStudio"),nullptr,LOAD_NoWarn);
    if(!Config)Config=LoadObject<UCustomizationStudioConfig>(nullptr,TEXT("/Game/FPS/Customizable/Character/Preview/DA_CustomizationStudio.DA_CustomizationStudio"));
    if(!TestNotNull(TEXT("Preview config"),Config))return false;
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto* Studio=World->SpawnActor<ACustomizationStudioActor>();Studio->Config=Config;
    auto* Controller=World->SpawnActor<ACustomizationStudioController>();Controller->Studio=Studio;
    const TArray<FString> Names={TEXT("Hair_S_UpdoBuns"),TEXT("Hair_M_BobStraight"),TEXT("Hair_M_SlickPonytail")};
    for(const FString Type:{TEXT("Seo"),TEXT("Custom"),TEXT("Fab")})
    {
        const auto Choices=Studio->HairChoices(Type);
        TestEqual(Type+TEXT(" has three hairstyles"),Choices.Num(),3);
        for(int32 I=0;I<FMath::Min(Choices.Num(),3);++I)
            TestEqual(Type+FString::Printf(TEXT(" number %d"),I+1),Choices[I].Groom->GetName(),Names[I]);
    }
    for(const FString From:{TEXT("Seo"),TEXT("Custom"),TEXT("Fab")})
    for(const FString To:{TEXT("Seo"),TEXT("Custom"),TEXT("Fab")})
    for(const auto& Choice:Studio->HairChoices(From))
    {
        Controller->Draft.BodyType=From;Controller->Draft.HairId=Choice.Id;
        Controller->SelectBody(To);
        const auto Destination=Studio->HairChoices(To);
        const auto* Match=Destination.FindByPredicate([&](const FStudioGroomChoice& H){return H.Id==Controller->Draft.HairId;});
        TestTrue(From+TEXT(" -> ")+To+TEXT(" preserves ")+Choice.Groom->GetName(),Match&&Match->Groom->GetFName()==Choice.Groom->GetFName());
    }
    Controller->Draft.HairId=TEXT("Bald");Controller->SelectBody(TEXT("Fab"));
    TestEqual(TEXT("Bald remains bald"),Controller->Draft.HairId,FName(TEXT("Bald")));
    World->DestroyWorld(false);
    return true;
}
#endif
