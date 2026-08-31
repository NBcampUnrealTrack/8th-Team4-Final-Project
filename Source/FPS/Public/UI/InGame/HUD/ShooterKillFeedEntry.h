// ShooterKillFeedEntry.h

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameMode/TeamTypes.h"
#include "GameplayTagContainer.h"
#include "ShooterKillFeedEntry.generated.h"

class UCommonTextBlock;

/**
 * Kill log 한줄에 필요한 스냅샷
 */
USTRUCT(BlueprintType)
struct FShooterKillFeedEntryData
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, Category = "Kill Feed")
	FText KillerName;
	UPROPERTY(BlueprintReadWrite, Category = "Kill Feed")
	FText VictimName;
	UPROPERTY(BlueprintReadWrite, Category = "Kill Feed")
	EGameTeam KillerTeam = EGameTeam::None;
	UPROPERTY(BlueprintReadWrite, Category = "Kill Feed")
	EGameTeam VictimTeam = EGameTeam::None;
	UPROPERTY(BlueprintReadWrite, Category = "Kill Feed")
	FGameplayTag WeaponTag;
	UPROPERTY(BlueprintReadWrite, Category = "Kill Feed")
	bool bHeadshot = false;
};

UCLASS()
class FPS_API UShooterKillFeedEntry : public UCommonUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "Kill Feed")
	void SetData(const FShooterKillFeedEntryData& InData);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Kill Feed")
	void RefreshTeamVisual(bool bKillerIsAlly, bool bVictimIsAlly);
	
protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_KillerName;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_VictimName;
	
	// 헤드샷 표시
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Icon_Headshot;
};
