#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameMode/DominationGameMode.h"
#include "GameMode/TrainingGameMode.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "GameMode/State/DominationGameState.h"
#include "Interfaces/RogueCombatClassSelector.h"
#include "Player/DominationPlayerController.h"
#include "Player/DominationPlayerState.h"
#include "Player/TrainingPlayerController.h"
#include "Training/TrainingPlayerComponent.h"
#include "UI/Frontend/CombatUIInitComponent.h"
#include "UI/Frontend/DominationUIInitComponent.h"
#include "UI/Frontend/TrainingUIInitComponent.h"
#include "UObject/UnrealType.h"

// Read-only class/default-object checks. These are NOT gameplay, asset or network tests.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingFrameworkModeSeparationTest,
    "FPS.Training.Framework.ModeSeparation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingFrameworkModeSeparationTest::RunTest(const FString&)
{
    TestTrue(TEXT("Training reuses the shooter mode"),
        ATrainingGameMode::StaticClass()->IsChildOf(AShooterGameMode::StaticClass()));
    TestFalse(TEXT("Training does not inherit capture rules"),
        ATrainingGameMode::StaticClass()->IsChildOf(ADominationGameMode::StaticClass()));
    const ATrainingGameMode* Defaults = GetDefault<ATrainingGameMode>();
    TestTrue(TEXT("Training uses the general team/killfeed state"),
        Defaults->GameStateClass == ABaseTeamGameState::StaticClass());
    TestFalse(TEXT("No Domination GameState default"),
        Defaults->GameStateClass->IsChildOf(ADominationGameState::StaticClass()));
    TestTrue(TEXT("Class state is reused, not duplicated"),
        Defaults->PlayerStateClass == ADominationPlayerState::StaticClass());
    TestTrue(TEXT("No hard-coded default combat pawn before selection"),
        Defaults->DefaultPawnClass == nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingFrameworkControllerSeparationTest,
    "FPS.Training.Framework.ControllerSeparation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingFrameworkControllerSeparationTest::RunTest(const FString&)
{
    TestTrue(TEXT("Shared shooting controller is the parent"),
        ATrainingPlayerController::StaticClass()->IsChildOf(AShooterPlayerController::StaticClass()));
    TestFalse(TEXT("No Domination input phase gate is inherited"),
        ATrainingPlayerController::StaticClass()->IsChildOf(ADominationPlayerController::StaticClass()));
    TestTrue(TEXT("Training default controller class"),
        GetDefault<ATrainingGameMode>()->PlayerControllerClass == ATrainingPlayerController::StaticClass());
    TestTrue(TEXT("Training class selection has no countdown"),
        GetDefault<ATrainingPlayerController>()->GetCombatClassSelectionTimeRemaining() < 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingFrameworkClassInterfaceTest,
    "FPS.Training.Framework.SharedClassCommands",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingFrameworkClassInterfaceTest::RunTest(const FString&)
{
    TestTrue(TEXT("Training implements the class-command interface"),
        ATrainingPlayerController::StaticClass()->ImplementsInterface(URogueCombatClassSelector::StaticClass()));
    TestTrue(TEXT("Domination implements the same interface"),
        ADominationPlayerController::StaticClass()->ImplementsInterface(URogueCombatClassSelector::StaticClass()));
    TestNotNull(TEXT("Training SelectCombatClass is reflected"),
        ATrainingPlayerController::StaticClass()->FindFunctionByName(TEXT("SelectCombatClass")));
    TestNotNull(TEXT("Domination SelectCombatClass remains reflected"),
        ADominationPlayerController::StaticClass()->FindFunctionByName(TEXT("SelectCombatClass")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingFrameworkUICompositionTest,
    "FPS.Training.Framework.UIComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingFrameworkUICompositionTest::RunTest(const FString&)
{
    TestTrue(TEXT("Training uses the common combat UI"),
        UTrainingUIInitComponent::StaticClass()->IsChildOf(UCombatUIInitComponent::StaticClass()));
    TestTrue(TEXT("Domination uses the same common combat UI"),
        UDominationUIInitComponent::StaticClass()->IsChildOf(UCombatUIInitComponent::StaticClass()));
    TestFalse(TEXT("Training does not inherit objective UI policy"),
        UTrainingUIInitComponent::StaticClass()->IsChildOf(UDominationUIInitComponent::StaticClass()));
    const ATrainingPlayerController* Defaults = GetDefault<ATrainingPlayerController>();
    TArray<UTrainingPlayerComponent*> DialogBridges;
    Defaults->GetComponents<UTrainingPlayerComponent>(DialogBridges);
    TestEqual(TEXT("Exactly one inherited training dialog bridge"), DialogBridges.Num(), 1);
    TArray<UCombatUIInitComponent*> UIComponents;
    Defaults->GetComponents<UCombatUIInitComponent>(UIComponents);
    TestEqual(TEXT("Exactly one common combat UI component"), UIComponents.Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingFrameworkUIPropertyCompatibilityTest,
    "FPS.Training.Framework.UIPropertyCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingFrameworkUIPropertyCompatibilityTest::RunTest(const FString&)
{
    const FName Names[] = {
        TEXT("RootLayoutClass"), TEXT("InitialWidgetClass"), TEXT("InitialWidgetLayer"),
        TEXT("ClassSelectionWidgetClass"), TEXT("InGameMenuClass"), TEXT("AugmentSelectClass"),
        TEXT("OptionsScreenClass"), TEXT("ScoreboardClass"),
        TEXT("OnAugmentDismissStateChanged"), TEXT("OnAugmentPresentationChanged")
    };
    for (const FName PropertyName : Names)
    {
        const FProperty* TrainingProperty = FindFProperty<FProperty>(UTrainingUIInitComponent::StaticClass(), PropertyName);
        const FProperty* DominationProperty = FindFProperty<FProperty>(UDominationUIInitComponent::StaticClass(), PropertyName);
        TestNotNull(*FString::Printf(TEXT("Training keeps %s"), *PropertyName.ToString()), TrainingProperty);
        TestNotNull(*FString::Printf(TEXT("Domination keeps %s"), *PropertyName.ToString()), DominationProperty);
        TestTrue(TEXT("Both resolve the same inherited property"), TrainingProperty && TrainingProperty == DominationProperty);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingFrameworkRuleHUDSuppressionTest,
    "FPS.Training.Framework.RuleHUDSuppression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTrainingFrameworkRuleHUDSuppressionTest::RunTest(const FString&)
{
    const FArrayProperty* Classes = FindFProperty<FArrayProperty>(
        UTrainingUIInitComponent::StaticClass(), TEXT("SuppressedHUDWidgetClasses"));
    if (TestNotNull(TEXT("Rule-widget class list is configurable"), Classes))
    {
        const UTrainingUIInitComponent* Defaults = GetDefault<UTrainingUIInitComponent>();
        FScriptArrayHelper Array(Classes, Classes->ContainerPtrToValuePtr<void>(Defaults));
        TestEqual(TEXT("Five known objective widget classes are excluded by default"), Array.Num(), 5);
    }
    TestNotNull(TEXT("BP-only rule panels can be excluded by name"),
        FindFProperty<FArrayProperty>(UTrainingUIInitComponent::StaticClass(), TEXT("SuppressedHUDWidgetNames")));
    return true;
}

#endif
