// ShooterKillFeed.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameMode/State/BaseTeamGameState.h" 
#include "ShooterKillFeed.generated.h"

class UShooterKillFeedEntry;
class UVerticalBox;
struct FShooterKillFeedEntryData;

UCLASS()
class FPS_API UShooterKillFeed : public UCommonUserWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeConstruct() override;
	
	UFUNCTION(BlueprintCallable, Category = "Kill Feed")
	void AddEntry(const FShooterKillFeedEntryData& Entry);
	
	UFUNCTION(BlueprintCallable, Category = "Kill Feed|Debug")
	void DebugAddFakeEntry();
protected:
	virtual void NativeDestruct() override;
	
	// 오래된 줄 갱신
	void RemoveOldestEntry();
	
	UFUNCTION()
	void HandlePlayerEliminated(const FShooterEliminationEvent& EliminationEvent);
	
	UPROPERTY(EditDefaultsOnly, Category = "Kill Feed", meta = (ClampMin = "1"))
	int32 MaxVisibleEntries = 5;
	UPROPERTY(EditDefaultsOnly, Category = "Kill Feed", meta = (ClampMin = "0.1"))
	float EntryLifetimeSeconds = 5.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Kill Feed")
	TSubclassOf<UShooterKillFeedEntry> EntryWidgetClass;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> Box_Entries;
	
private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UShooterKillFeedEntry>> ActiveEntries;
	
	TWeakObjectPtr<ABaseTeamGameState> BoundGameState;
	
	FTimerHandle ExpiryTimerHandle;
};
