// ShooterKillFeedEntry.h

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameMode/Types/TeamTypes.h"
#include "GameplayTagContainer.h"
#include "ShooterKillFeedEntry.generated.h"

class UCommonTextBlock;
class UShooterWeaponDisplaySet;

// Feed가 어떤 종류의 죽음인지 판단
UENUM(BlueprintType)
enum class EShooterKillFeedEventType : uint8
{
	// 다른 플레이어가 죽임
	Elimination,
	// 스스로 죽음 Killer == Victim
	SelfElimination,
	// 가해자 특정 불가능 낙사/환경 피해
	Environment
};

// Kill log 한줄에 필요한 스냅샷
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
	UPROPERTY(BlueprintReadWrite, Category = "Kill Feed")
	EShooterKillFeedEventType EventType = EShooterKillFeedEventType::Elimination;
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
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Kill Feed")
	void BP_OnEntryDataApplied(const FShooterKillFeedEntryData& Data);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_KillerName;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_VictimName;

	// 헤드샷 표시
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Icon_Headshot;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kill Feed")
	TObjectPtr<UShooterWeaponDisplaySet> WeaponDisplaySet;
};
