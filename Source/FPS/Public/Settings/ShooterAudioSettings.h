#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ShooterAudioSettings.generated.h"

class USoundBase;
class USoundClass;
class USoundMix;

/** Assets for the local player's persistent volume modifier (not the project's base mix). */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Shooter Audio Settings"))
class FPS_API UShooterAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override;

	// Dedicated modifier; set Duration to -1 so it stays active until the player exits.
	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundMix> UserSettingsSoundMix;

	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundClass> SoundEffectsSoundClass;

	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundClass> UserInterfaceSoundClass;

	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundClass> VoiceSoundClass;

	// 처치자 본인에게만 재생되는 히트마커/킬 피드백 사운드.
	// BP_ShooterPlayerController/BP_BombPlayerController/BP_DominationPlayerController 등
	// PlayerController 하위 클래스마다 따로 채워야 했던 걸 여기 하나로 통합한다.
	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback")
	TSoftObjectPtr<USoundBase> HitMarkerSound_Body;

	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback")
	TSoftObjectPtr<USoundBase> HitMarkerSound_Shield;

	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback")
	TSoftObjectPtr<USoundBase> HitMarkerSound_Headshot;

	// 처치 강조 사운드. 비어있으면 부위별 사운드로 대체된다.
	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback")
	TSoftObjectPtr<USoundBase> HitMarkerSound_Kill;

	// 처치 확정 시 재생되는 별도의 킬 사운드(히트마커와 무관한 경로, ClientPlayKillSound 참고).
	// 멀티킬이 아닌 일반 킬(스트릭 1)의 기본값이자, 스트릭별 사운드가 비어있을 때의 폴백이기도 하다.
	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback")
	TSoftObjectPtr<USoundBase> LocalKillSound;

	// 멀티킬 사운드. 직전 킬로부터 AShooterPlayerState::MultiKillWindowSeconds(기본 5초) 이내에
	// 또 킬하면 스트릭에 맞춰 재생된다. 비어있으면 LocalKillSound로 대체.
	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback")
	TSoftObjectPtr<USoundBase> DoubleKillSound;

	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback")
	TSoftObjectPtr<USoundBase> TripleKillSound;

	// 4연킬 이상은 전부 이 사운드로 재생(펜타킬 등 그 이상 전용 사운드는 없음).
	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback")
	TSoftObjectPtr<USoundBase> QuadraKillSound;
};
