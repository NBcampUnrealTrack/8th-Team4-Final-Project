// ShooterOutGameSubsystem.cpp
#include "UI/OutGame/ShooterOutGameSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Settings/ShooterAudioSettings.h"
#include "Settings/ShooterGameUserSettings.h"
#include "UI/ShooterUISettings.h"

namespace
{
	constexpr float MusicFadeSeconds = 1.0f;

	bool IsAudible(const UAudioComponent* Music)
	{
		if (!IsValid(Music))
		{
			return false;
		}

		const EAudioComponentPlayState PlayState = Music->GetPlayState();
		return PlayState == EAudioComponentPlayState::Playing
			|| PlayState == EAudioComponentPlayState::FadingIn;
	}
}

void UShooterOutGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SeamlessTravelHandle = FWorldDelegates::OnSeamlessTravelStart.AddUObject(
		this, &ThisClass::HandleSeamlessTravelStart);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &ThisClass::HandlePostLoadMapWithWorld);

	if (UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get())
	{
		Settings->OnAudioSettingsChanged.AddUObject(this, &ThisClass::HandleAudioSettingsChanged);
	}
}

void UShooterOutGameSubsystem::Deinitialize()
{
	if (UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get())
	{
		Settings->OnAudioSettingsChanged.RemoveAll(this);
	}

	if (SeamlessTravelHandle.IsValid())
	{
		FWorldDelegates::OnSeamlessTravelStart.Remove(SeamlessTravelHandle);
		SeamlessTravelHandle.Reset();
	}

	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	if (IsValid(ActiveMusic))
	{
		ActiveMusic->Stop();
	}
	if (IsValid(FadingMusic))
	{
		FadingMusic->Stop();
	}
	ActiveMusic = nullptr;
	FadingMusic = nullptr;

	Super::Deinitialize();
}

bool UShooterOutGameSubsystem::HasShownStartupIntro() const
{
	return bHasShownStartupIntro;
}

void UShooterOutGameSubsystem::MarkStartupIntroShown()
{
	bHasShownStartupIntro = true;
}

void UShooterOutGameSubsystem::SetMusicState(const EShooterOutGameMusicState NewState)
{
	USoundBase* NewMusic = nullptr;
	if (NewState != EShooterOutGameMusicState::None)
	{
		if (const TSoftObjectPtr<USoundBase>* Entry = GetDefault<UShooterUISettings>()->OutGameMusic.Find(NewState))
		{
			NewMusic = Entry->LoadSynchronous();
		}
	}

	// 같은 곡이면 이어 틀되, 옵션에서 바뀐 음악 볼륨/라우팅은 즉시 갱신한다.
	if (IsAudible(ActiveMusic) && ActiveMusic->Sound == NewMusic)
	{
		ApplyMusicSettingsToComponent(ActiveMusic);
		return;
	}

	if (IsAudible(ActiveMusic))
	{
		if (IsValid(FadingMusic))
		{
			FadingMusic->Stop();
		}
		ActiveMusic->FadeOut(MusicFadeSeconds, 0.0f);
		FadingMusic = ActiveMusic;
	}
	ActiveMusic = nullptr;

	if (!IsValid(NewMusic))
	{
		return;
	}

	const float MusicComponentVolume = ResolveMusicComponentVolume();
	ActiveMusic = UGameplayStatics::CreateSound2D(
		this, NewMusic, MusicComponentVolume, 1.0f, 0.0f, nullptr, true, false);
	if (IsValid(ActiveMusic))
	{
		ApplyMusicSettingsToComponent(ActiveMusic);
		ActiveMusic->FadeIn(MusicFadeSeconds, MusicComponentVolume);
	}
}

void UShooterOutGameSubsystem::HandleAudioSettingsChanged()
{
	ApplyMusicSettingsToComponent(ActiveMusic);
}

void UShooterOutGameSubsystem::ApplyMusicSettingsToComponent(UAudioComponent* MusicComponent) const
{
	if (!IsValid(MusicComponent))
	{
		return;
	}

	const UShooterAudioSettings* AudioSettings = GetDefault<UShooterAudioSettings>();
	if (USoundClass* MusicClass = AudioSettings->MusicSoundClass.LoadSynchronous())
	{
		MusicComponent->SoundClassOverride = MusicClass;
	}

	MusicComponent->SetVolumeMultiplier(ResolveMusicComponentVolume());
}

float UShooterOutGameSubsystem::ResolveMusicComponentVolume() const
{
	const UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
	if (!IsValid(Settings))
	{
		return 1.0f;
	}

	const UShooterAudioSettings* AudioSettings = GetDefault<UShooterAudioSettings>();
	const bool bHasMusicClass = IsValid(AudioSettings->MusicSoundClass.LoadSynchronous());
	const bool bHasUserMix = IsValid(AudioSettings->UserSettingsSoundMix.LoadSynchronous());

	// 정상 설정이면 Music SoundClass/SoundMix를 사용하고, 누락 시 컴포넌트 볼륨으로 보정한다.
	return bHasMusicClass && bHasUserMix
		? 1.0f
		: FMath::Clamp(Settings->GetAudioVolume(EShooterAudioChannel::Music), 0.0f, 2.0f);
}

void UShooterOutGameSubsystem::HandleSeamlessTravelStart(UWorld* DepartingWorld, const FString& DestinationMap)
{
	if (!IsValid(DepartingWorld) || DepartingWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	if (!IsOutGameMusicMap(DestinationMap))
	{
		SetMusicState(EShooterOutGameMusicState::None);
	}
}

void UShooterOutGameSubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (!IsValid(LoadedWorld) || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	if (!IsOutGameMusicMap(LoadedWorld->GetPackage()->GetName()))
	{
		SetMusicState(EShooterOutGameMusicState::None);
	}
}

bool UShooterOutGameSubsystem::IsOutGameMusicMap(const FString& MapName) const
{
	const FString ShortName = FPackageName::GetShortName(UWorld::RemovePIEPrefix(MapName));
	for (const TSoftObjectPtr<UWorld>& Map : GetDefault<UShooterUISettings>()->OutGameMusicMaps)
	{
		if (Map.GetAssetName() == ShortName)
		{
			return true;
		}
	}
	return false;
}
