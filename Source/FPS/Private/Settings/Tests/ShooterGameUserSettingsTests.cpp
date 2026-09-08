#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Settings/ShooterGameUserSettings.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	constexpr EShooterAudioChannel AudioChannels[] =
	{
		EShooterAudioChannel::Master,
		EShooterAudioChannel::Music,
		EShooterAudioChannel::SoundEffects,
		EShooterAudioChannel::UserInterface,
		EShooterAudioChannel::Voice
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShooterInputSettingsValuesTest,
	"FPS.Settings.UserSettings.InputValues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterInputSettingsValuesTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UShooterGameUserSettings> Settings(
		NewObject<UShooterGameUserSettings>(GetTransientPackage(), NAME_None, RF_Transient));
	Settings->SetToDefaults();

	Settings->SetMouseHorizontalSensitivity(-1.0f);
	TestEqual(TEXT("Horizontal sensitivity clamps to minimum"), Settings->GetMouseHorizontalSensitivity(), 0.01f);
	Settings->SetMouseHorizontalSensitivity(20.0f);
	TestEqual(TEXT("Horizontal sensitivity clamps to maximum"), Settings->GetMouseHorizontalSensitivity(), 10.0f);
	Settings->SetMouseHorizontalSensitivity(2.5f);
	TestEqual(TEXT("Horizontal sensitivity retains an in-range value"), Settings->GetMouseHorizontalSensitivity(), 2.5f);
	TestEqual(TEXT("Horizontal change does not change vertical sensitivity"), Settings->GetMouseVerticalSensitivity(), 1.0f);

	Settings->SetMouseVerticalSensitivity(-1.0f);
	TestEqual(TEXT("Vertical sensitivity clamps to minimum"), Settings->GetMouseVerticalSensitivity(), 0.01f);
	Settings->SetMouseVerticalSensitivity(20.0f);
	TestEqual(TEXT("Vertical sensitivity clamps to maximum"), Settings->GetMouseVerticalSensitivity(), 10.0f);
	Settings->SetMouseVerticalSensitivity(3.5f);
	TestEqual(TEXT("Vertical sensitivity retains an in-range value"), Settings->GetMouseVerticalSensitivity(), 3.5f);
	TestEqual(TEXT("Vertical change does not change horizontal sensitivity"), Settings->GetMouseHorizontalSensitivity(), 2.5f);

	Settings->SetAimSensitivityMultiplier(-1.0f);
	TestEqual(TEXT("ADS multiplier clamps to minimum"), Settings->GetAimSensitivityMultiplier(), 0.01f);
	Settings->SetAimSensitivityMultiplier(3.0f);
	TestEqual(TEXT("ADS multiplier clamps to maximum"), Settings->GetAimSensitivityMultiplier(), 2.0f);
	Settings->SetAimSensitivityMultiplier(0.5f);
	TestEqual(TEXT("ADS multiplier retains an in-range value"), Settings->GetAimSensitivityMultiplier(), 0.5f);

	Settings->SetInvertVerticalLookEnabled(true);
	Settings->SetDynamicCrosshairEnabled(false);
	TestTrue(TEXT("Vertical inversion can be enabled"), Settings->IsInvertVerticalLookEnabled());
	TestFalse(TEXT("Dynamic crosshair can be disabled"), Settings->IsDynamicCrosshairEnabled());
	Settings->SetInvertVerticalLookEnabled(false);
	Settings->SetDynamicCrosshairEnabled(true);
	TestFalse(TEXT("Vertical inversion can be disabled"), Settings->IsInvertVerticalLookEnabled());
	TestTrue(TEXT("Dynamic crosshair can be enabled"), Settings->IsDynamicCrosshairEnabled());
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShooterLookSensitivityApplicationTest,
	"FPS.Settings.UserSettings.LookSensitivityApplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterLookSensitivityApplicationTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UShooterGameUserSettings> Settings(
		NewObject<UShooterGameUserSettings>(GetTransientPackage(), NAME_None, RF_Transient));
	Settings->SetToDefaults();
	Settings->SetMouseHorizontalSensitivity(2.0f);
	Settings->SetMouseVerticalSensitivity(3.0f);
	Settings->SetAimSensitivityMultiplier(0.5f);
	Settings->SetInvertVerticalLookEnabled(true);

	const FVector2D Hip = Settings->ApplyLookSensitivity(FVector2D(1.0f, 2.0f), false);
	TestEqual(TEXT("Hip look applies horizontal sensitivity"), Hip.X, 2.0);
	TestEqual(TEXT("Hip look applies vertical sensitivity and invert-Y"), Hip.Y, -6.0);

	const FVector2D Ads = Settings->ApplyLookSensitivity(FVector2D(1.0f, 2.0f), true);
	TestEqual(TEXT("ADS look applies the ADS multiplier to X"), Ads.X, 1.0);
	TestEqual(TEXT("ADS look applies the ADS multiplier to Y"), Ads.Y, -3.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShooterSmartTuningSettingsTest,
	"FPS.Settings.UserSettings.SmartTuning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterSmartTuningSettingsTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UShooterGameUserSettings> Settings(
		NewObject<UShooterGameUserSettings>(GetTransientPackage(), NAME_None, RF_Transient));
	Settings->SetToDefaults();

	TestEqual(TEXT("Default language is Korean"), Settings->GetLanguage(), EShooterOptionsLanguage::Korean);
	TestEqual(TEXT("Default tuning profile is Balanced120"), Settings->GetExperienceProfile(), EShooterExperienceProfile::Balanced120);
	TestEqual(TEXT("Default performance target is 120 FPS"), Settings->GetPerformanceTargetFPS(), 120.0f);

	Settings->SetPerformanceTargetFPS(10.0f);
	TestEqual(TEXT("Performance target clamps to 30 FPS"), Settings->GetPerformanceTargetFPS(), 30.0f);
	Settings->SetPerformanceTargetFPS(2000.0f);
	TestEqual(TEXT("Performance target clamps to 1000 FPS"), Settings->GetPerformanceTargetFPS(), 1000.0f);
	Settings->SetPerformanceTargetFPS(240.0f);
	TestEqual(TEXT("Performance target keeps supported in-range values"), Settings->GetPerformanceTargetFPS(), 240.0f);

	Settings->SetExperienceProfile(EShooterExperienceProfile::Competitive144);
	TestEqual(TEXT("Experience profile stores independently from target FPS"), Settings->GetExperienceProfile(), EShooterExperienceProfile::Competitive144);
	TestEqual(TEXT("Changing profile does not overwrite target FPS"), Settings->GetPerformanceTargetFPS(), 240.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShooterAudioSettingsChannelsTest,
	"FPS.Settings.UserSettings.AudioChannels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterAudioSettingsChannelsTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UShooterGameUserSettings> Settings(
		NewObject<UShooterGameUserSettings>(GetTransientPackage(), NAME_None, RF_Transient));

	for (const EShooterAudioChannel Channel : AudioChannels)
	{
		Settings->SetToDefaults();
		Settings->SetAudioVolume(Channel, 0.25f);
		TestEqual(
			FString::Printf(TEXT("Channel %d retains its selected volume"), static_cast<int32>(Channel)),
			Settings->GetAudioVolume(Channel), 0.25f);

		for (const EShooterAudioChannel OtherChannel : AudioChannels)
		{
			if (OtherChannel != Channel)
			{
				TestEqual(
					FString::Printf(TEXT("Changing channel %d leaves channel %d unchanged"),
						static_cast<int32>(Channel), static_cast<int32>(OtherChannel)),
					Settings->GetAudioVolume(OtherChannel), 1.0f);
			}
		}

		Settings->SetAudioVolume(Channel, -1.0f);
		TestEqual(
			FString::Printf(TEXT("Channel %d clamps to zero"), static_cast<int32>(Channel)),
			Settings->GetAudioVolume(Channel), 0.0f);
		Settings->SetAudioVolume(Channel, 2.0f);
		TestEqual(
			FString::Printf(TEXT("Channel %d clamps to one"), static_cast<int32>(Channel)),
			Settings->GetAudioVolume(Channel), 1.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShooterAudioSettingsNotificationTest,
	"FPS.Settings.UserSettings.AudioChangeNotification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterAudioSettingsNotificationTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UShooterGameUserSettings> Settings(
		NewObject<UShooterGameUserSettings>(GetTransientPackage(), NAME_None, RF_Transient));
	Settings->SetToDefaults();

	int32 ChangeCount = 0;
	float ObservedMusicVolume = 1.0f;
	bool bObservedMuted = false;
	bool bObservedVoiceEnabled = true;
	const FDelegateHandle Handle = Settings->OnAudioSettingsChanged.AddLambda([&]()
	{
		++ChangeCount;
		ObservedMusicVolume = Settings->GetAudioVolume(EShooterAudioChannel::Music);
		bObservedMuted = Settings->IsMasterAudioMuted();
		bObservedVoiceEnabled = Settings->IsVoiceChatEnabled();
	});

	Settings->SetAudioVolume(EShooterAudioChannel::Music, 0.4f);
	TestEqual(TEXT("Volume change emits one notification"), ChangeCount, 1);
	TestEqual(TEXT("Listener sees the new volume"), ObservedMusicVolume, 0.4f);
	Settings->SetAudioVolume(EShooterAudioChannel::Music, 0.4f);
	TestEqual(TEXT("Repeated volume emits no notification"), ChangeCount, 1);

	Settings->SetMasterAudioMuted(true);
	TestEqual(TEXT("Mute change emits one notification"), ChangeCount, 2);
	TestTrue(TEXT("Listener sees the new mute state"), bObservedMuted);
	Settings->SetMasterAudioMuted(true);
	TestEqual(TEXT("Repeated mute emits no notification"), ChangeCount, 2);

	Settings->SetVoiceChatEnabled(false);
	TestEqual(TEXT("Voice toggle emits one notification"), ChangeCount, 3);
	TestFalse(TEXT("Listener sees the new voice state"), bObservedVoiceEnabled);
	Settings->SetVoiceChatEnabled(false);
	TestEqual(TEXT("Repeated voice toggle emits no notification"), ChangeCount, 3);

	Settings->SetAudioVolume(EShooterAudioChannel::Music, -1.0f);
	TestEqual(TEXT("Clamped volume change emits one notification"), ChangeCount, 4);
	TestEqual(TEXT("Listener sees the clamped volume"), ObservedMusicVolume, 0.0f);
	Settings->SetAudioVolume(EShooterAudioChannel::Music, -2.0f);
	TestEqual(TEXT("Same clamped result emits no notification"), ChangeCount, 4);

	Settings->OnAudioSettingsChanged.Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShooterSettingsDefaultsTest,
	"FPS.Settings.UserSettings.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UShooterGameUserSettings> Settings(
		NewObject<UShooterGameUserSettings>(GetTransientPackage(), NAME_None, RF_Transient));
	Settings->SetToDefaults();
	Settings->SetDynamicCrosshairEnabled(false);
	Settings->SetMouseHorizontalSensitivity(2.0f);
	Settings->SetMouseVerticalSensitivity(3.0f);
	Settings->SetAimSensitivityMultiplier(0.5f);
	Settings->SetInvertVerticalLookEnabled(true);
	for (const EShooterAudioChannel Channel : AudioChannels)
	{
		Settings->SetAudioVolume(Channel, 0.25f);
	}
	Settings->SetMasterAudioMuted(true);
	Settings->SetVoiceChatEnabled(false);
	Settings->SetLanguage(EShooterOptionsLanguage::English);
	Settings->SetExperienceProfile(EShooterExperienceProfile::MaxQuality);
	Settings->SetPerformanceTargetFPS(360.0f);

	int32 ChangeCount = 0;
	bool bListenerSawAudioDefaults = false;
	const FDelegateHandle Handle = Settings->OnAudioSettingsChanged.AddLambda([&]()
	{
		++ChangeCount;
		bListenerSawAudioDefaults = !Settings->IsMasterAudioMuted() && Settings->IsVoiceChatEnabled();
		for (const EShooterAudioChannel Channel : AudioChannels)
		{
			bListenerSawAudioDefaults &= Settings->GetAudioVolume(Channel) == 1.0f;
		}
	});

	Settings->SetToDefaults();
	Settings->ApplyLanguageCulture();
	TestEqual(TEXT("Reset emits one notification for the complete audio state"), ChangeCount, 1);
	TestTrue(TEXT("Listener sees all audio defaults after reset"), bListenerSawAudioDefaults);
	TestTrue(TEXT("Reset enables dynamic crosshair"), Settings->IsDynamicCrosshairEnabled());
	TestEqual(TEXT("Reset restores horizontal sensitivity"), Settings->GetMouseHorizontalSensitivity(), 1.0f);
	TestEqual(TEXT("Reset restores vertical sensitivity"), Settings->GetMouseVerticalSensitivity(), 1.0f);
	TestEqual(TEXT("Reset restores ADS multiplier"), Settings->GetAimSensitivityMultiplier(), 1.0f);
	TestFalse(TEXT("Reset disables vertical inversion"), Settings->IsInvertVerticalLookEnabled());
	for (const EShooterAudioChannel Channel : AudioChannels)
	{
		TestEqual(
			FString::Printf(TEXT("Reset restores channel %d volume"), static_cast<int32>(Channel)),
			Settings->GetAudioVolume(Channel), 1.0f);
	}
	TestFalse(TEXT("Reset unmutes master audio"), Settings->IsMasterAudioMuted());
	TestTrue(TEXT("Reset enables voice chat"), Settings->IsVoiceChatEnabled());
	TestEqual(TEXT("Reset restores Korean language"), Settings->GetLanguage(), EShooterOptionsLanguage::Korean);
	TestEqual(TEXT("Reset restores Balanced120 profile"), Settings->GetExperienceProfile(), EShooterExperienceProfile::Balanced120);
	TestEqual(TEXT("Reset restores 120 FPS performance target"), Settings->GetPerformanceTargetFPS(), 120.0f);

	Settings->OnAudioSettingsChanged.Remove(Handle);
	return true;
}

#endif
