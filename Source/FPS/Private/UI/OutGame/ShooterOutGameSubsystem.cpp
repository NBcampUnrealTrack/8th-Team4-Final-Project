// ShooterOutGameSubsystem.cpp
#include "UI/OutGame/ShooterOutGameSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Sound/SoundBase.h"
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
}

void UShooterOutGameSubsystem::Deinitialize()
{
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

	// 같은 곡이면 이어 튼다
	if (IsAudible(ActiveMusic) && ActiveMusic->Sound == NewMusic)
	{
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

	ActiveMusic = UGameplayStatics::CreateSound2D(
		this, NewMusic, 1.0f, 1.0f, 0.0f, nullptr, true, false);
	if (IsValid(ActiveMusic))
	{
		ActiveMusic->FadeIn(MusicFadeSeconds);
	}
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
