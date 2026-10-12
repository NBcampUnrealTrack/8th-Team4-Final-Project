#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameMode/Types/DominationTypes.h"
#include "ShooterAudioSettings.generated.h"

class USoundBase;
class USoundClass;
class USoundConcurrency;
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

	// 공격자에게 들리는 타격음. 기존 Config 키는 유지하고 에디터 표시명만 구분한다.
	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback", meta = (DisplayName = "Impact Sound Body", ToolTip = "Body impact sound. Plays alongside Hit Marker Sound."))
	TSoftObjectPtr<USoundBase> HitMarkerSound_Body;

	// 실드 전용 타격음은 기존 기획대로 재생하지 않는다.
	UPROPERTY(Config, EditAnywhere, AdvancedDisplay, Category = "Combat Feedback", meta = (DisplayName = "Impact Sound Shield", ToolTip = "Reserved setting. Shield-specific impact audio is disabled; Hit Marker Sound still confirms the hit."))
	TSoftObjectPtr<USoundBase> HitMarkerSound_Shield;

	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback", meta = (DisplayName = "Impact Sound Headshot", ToolTip = "Headshot impact sound. Plays alongside Hit Marker Sound."))
	TSoftObjectPtr<USoundBase> HitMarkerSound_Headshot;

	// 처치 시 타격음 대체값. 비우면 맞은 부위의 타격음을 그대로 사용한다.
	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback", meta = (DisplayName = "Impact Sound Kill", ToolTip = "Optional kill impact override. Leave empty to use the body or headshot impact. Put the confirmation ding in Hit Marker Sound."))
	TSoftObjectPtr<USoundBase> HitMarkerSound_Kill;

	// 헤드샷 처치 타격음. 비어있으면 Kill, 그마저 비어있으면 Headshot을 사용한다.
	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback", meta = (DisplayName = "Impact Sound Headshot Kill", ToolTip = "Optional headshot kill impact override. Falls back to Impact Sound Kill, then Impact Sound Headshot."))
	TSoftObjectPtr<USoundBase> HitMarkerSound_HeadshotKill;

	// 명중 확인음(띵). 처치 여부와 관계없이 선택된 타격음과 함께 재생한다.
	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback", meta = (DisplayName = "Hit Marker Sound", ToolTip = "Confirmation sound played alongside the selected impact on every audible hit, including kills and shield hits. Leave empty to disable this layer."))
	TSoftObjectPtr<USoundBase> HitMarkerSound_Confirmation;

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

	// 팀원이 위치를 핑 찍었을 때(크로스헤어가 지형/오브젝트를 맞춤) 재생.
	UPROPERTY(Config, EditAnywhere, Category = "Ping")
	TSoftObjectPtr<USoundBase> PingSound_Location;

	// 팀원이 적을 핑 찍었을 때(크로스헤어가 적 캐릭터를 맞춤) 재생. 비어있으면 PingSound_Location으로 대체.
	UPROPERTY(Config, EditAnywhere, Category = "Ping")
	TSoftObjectPtr<USoundBase> PingSound_Enemy;

	// 피격 시 재생되는 고통 신음(맞는 소리). 전투 클래스별로 배열 중 하나를 랜덤 재생한다.
	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback|Pain")
	TArray<TSoftObjectPtr<USoundBase>> PainSounds_Trooper;

	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback|Pain")
	TArray<TSoftObjectPtr<USoundBase>> PainSounds_Ranger;

	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback|Pain")
	TArray<TSoftObjectPtr<USoundBase>> PainSounds_Vanguard;

	// 전투 클래스를 알 수 없을 때(클래스 미선택, Domination 외 모드 등) 쓰는 공용 폴백.
	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback|Pain")
	TArray<TSoftObjectPtr<USoundBase>> PainSounds_Default;

	// CombatClass에 맞는 고통 사운드 배열을 반환한다. 해당 클래스 배열이 비어있으면
	// PainSounds_Default로 대체한다. 서버(재생할 사운드 인덱스 선택)와 각 클라이언트
	// (Multicast로 받은 인덱스로 실제 에셋 로드) 양쪽에서 동일 로직을 쓰기 위해 여기 하나로 둔다.
	const TArray<TSoftObjectPtr<USoundBase>>& ResolvePainSoundPool(EDominationCombatClass CombatClass) const;

	// 스태미너 소진으로 달리기가 끊겼을 때 본인에게만 재생되는 숨 고르는 소리
	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback|Stamina")
	TArray<TSoftObjectPtr<USoundBase>> ExhaustedBreathSounds_Trooper;

	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback|Stamina")
	TArray<TSoftObjectPtr<USoundBase>> ExhaustedBreathSounds_Ranger;

	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback|Stamina")
	TArray<TSoftObjectPtr<USoundBase>> ExhaustedBreathSounds_Vanguard;

	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback|Stamina")
	TArray<TSoftObjectPtr<USoundBase>> ExhaustedBreathSounds_Default;

	// 숨소리끼리 겹치지 않게 재생 시 넘기는 동시 재생 제한
	UPROPERTY(Config, EditAnywhere, Category = "Combat Feedback|Stamina")
	TSoftObjectPtr<USoundConcurrency> ExhaustedBreathConcurrency;

	const TArray<TSoftObjectPtr<USoundBase>>& ResolveExhaustedBreathSoundPool(EDominationCombatClass CombatClass) const;
};
