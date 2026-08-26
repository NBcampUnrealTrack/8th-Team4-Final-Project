// ShooterMatchResulTypes.h
#pragma once

#include "CoreMinimal.h"
#include "GameMode/TeamTypes.h"
#include "ShooterMatchResultTypes.generated.h"

// 결과화면 플레이어 Row
USTRUCT()
struct FShooterMatchResultRow
{
	GENERATED_BODY()
	
	FText PlayerName;
	EGameTeam Team = EGameTeam::None;

	int32 Kills = 0;
	int32 Deaths = 0;
	int32 Assists = 0;
};

// 매치 종료 시점의 결과를 담아 맵 이동해 로비로 가져가는 값
// 복제하지않고 각 클라이언트가 이미 복제받은 값으로 만듦
USTRUCT()
struct FShooterMatchResultSnapshot
{
	GENERATED_BODY()
	
	// true면 그릴 수 있는 결과가 있다라고 판정
	bool bValid = false;
	
	// 매치 종료 시 내 팀
	EGameTeam LocalTeam = EGameTeam::None;
	EGameTeam MatchWinner = EGameTeam::None;
	
	int32 TeamAScore = 0;
	int32 TeamBScore = 0;
	
	// KDA Collum을 띄울지
	bool bStatsAvailable = false;
	TArray<FShooterMatchResultRow> Rows;

	void Reset() { *this = FShooterMatchResultSnapshot(); }
};
