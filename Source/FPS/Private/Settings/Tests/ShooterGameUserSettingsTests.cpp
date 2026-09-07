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

	Settings->OnAudioSettingsChanged.Remove(Handle);
	return true;
}

#endif
