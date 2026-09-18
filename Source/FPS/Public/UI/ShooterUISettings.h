// ShooterUISettings.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameMode/Types/LobbyTypes.h"
#include "UI/OutGame/ShooterOutGameTypes.h"
#include "GameMode/Types/TeamTypes.h"
#include "ShooterUISettings.generated.h"

class UShooterMatchResultScreen;
class USoundBase;
class UTexture2D;
class UWorld;

// 방 화면 모드 패널에 그릴 값
USTRUCT(BlueprintType)
struct FShooterLobbyModeDisplay
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Lobby")
	ELobbyMatchMode Mode = ELobbyMatchMode::Bomb;

	UPROPERTY(Config, EditAnywhere, Category = "Lobby")
	FText ModeName;

	UPROPERTY(Config, EditAnywhere, Category = "Lobby")
	FText MapName;

	UPROPERTY(Config, EditAnywhere, Category = "Lobby")
	TSoftObjectPtr<UTexture2D> Thumbnail;
};

/**
 * MatchResultScreen에서 참조해야하는 에셋과 UI 색 설정
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Shooter UI"))
class FPS_API UShooterUISettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	// 매치가 끝나고 로비에서 띄울 결과 화면
	UPROPERTY(Config, EditAnywhere, Category = "Match Result")
	TSoftClassPtr<UShooterMatchResultScreen> MatchResultScreenClass;
	
	UFUNCTION(BlueprintPure, Category = "UI|Theme")
	static FLinearColor GetAllyColor();
	
	UFUNCTION(BlueprintPure, Category = "UI|Theme")
	static FLinearColor GetEnemyColor();
	
	UPROPERTY(Config, EditAnywhere, Category = "Team")
	FLinearColor AllyColor = FLinearColor(0.2f, 0.6f, 1.0f);
	
	UPROPERTY(Config, EditAnywhere, Category = "Team")
	FLinearColor EnemyColor = FLinearColor(1.0f, 0.3f, 0.3f);

	// 배열 순서가 곧 화살표로 넘기는 순서. TMap 은 순회 순서가 보장되지 않음
	UPROPERTY(Config, EditAnywhere, Category = "Lobby")
	TArray<FShooterLobbyModeDisplay> LobbyModes;

	// 로비 A/B 는 아군/적군이 아님. 누가 보든 같은 팀이 같은 색이라 인게임 색과 별개 토큰
	UFUNCTION(BlueprintPure, Category = "UI|Theme")
	static FLinearColor GetLobbyTeamColor(EGameTeam Team);

	UPROPERTY(Config, EditAnywhere, Category = "Lobby")
	FLinearColor LobbyTeamAColor = FLinearColor(0.2f, 0.6f, 1.0f);

	UPROPERTY(Config, EditAnywhere, Category = "Lobby")
	FLinearColor LobbyTeamBColor = FLinearColor(1.0f, 0.3f, 0.3f);

	// 상태별 BGM. 두 상태에 같은 에셋이면 맵이 바뀌어도 이어진다
	UPROPERTY(Config, EditAnywhere, Category = "OutGame Music")
	TMap<EShooterOutGameMusicState, TSoftObjectPtr<USoundBase>> OutGameMusic;

	// BGM 을 유지하는 맵. 그 밖의 맵으로 떠나거나 로드되면 끈다
	UPROPERTY(Config, EditAnywhere, Category = "OutGame Music")
	TArray<TSoftObjectPtr<UWorld>> OutGameMusicMaps;
};
