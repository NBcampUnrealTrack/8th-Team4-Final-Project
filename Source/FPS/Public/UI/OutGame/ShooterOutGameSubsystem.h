// ShooterOutGameSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/OutGame/ShooterOutGameTypes.h"
#include "ShooterOutGameSubsystem.generated.h"

class UAudioComponent;
class UWorld;

/**
 * BGM위함
 */
UCLASS()
class FPS_API UShooterOutGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool HasShownStartupIntro() const;
	void MarkStartupIntroShown();

	void SetMusicState(EShooterOutGameMusicState NewState);

private:
	void HandleSeamlessTravelStart(UWorld* DepartingWorld, const FString& DestinationMap);
	void HandlePostLoadMapWithWorld(UWorld* LoadedWorld);
	bool IsOutGameMusicMap(const FString& MapName) const;
	void HandleAudioSettingsChanged();
	void ApplyMusicSettingsToComponent(UAudioComponent* MusicComponent) const;
	float ResolveMusicComponentVolume() const;

	bool bHasShownStartupIntro = false;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveMusic;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> FadingMusic;

	FDelegateHandle SeamlessTravelHandle;
	FDelegateHandle PostLoadMapHandle;
};
