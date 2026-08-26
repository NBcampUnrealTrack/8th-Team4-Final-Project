// ShooterMatchResultSetting.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ShooterMatchResultSetting.generated.h"

class UShooterMatchResultScreen;

/**
 * MatchResultScreen에서 참조해야하는 에셋 모으는 설정
 * 확장 가능성이 없다고 판단되어 전용으로 만듦
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Shooter UI"))
class FPS_API UShooterMatchResultSetting : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	// 매치가 끝나고 로비에서 띄울 결과 화면
	UPROPERTY(Config, EditAnywhere, Category = "Match Result")
	TSoftClassPtr<UShooterMatchResultScreen> MatchResultScreenClass;
};
