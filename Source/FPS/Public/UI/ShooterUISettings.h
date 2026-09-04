// ShooterUISettings.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ShooterUISettings.generated.h"

class UShooterMatchResultScreen;

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
};
