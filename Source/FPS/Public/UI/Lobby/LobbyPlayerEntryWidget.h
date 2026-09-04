#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/TeamTypes.h"
#include "Blueprint/UserWidget.h"
#include "LobbyPlayerEntryWidget.generated.h"

class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class FPS_API ULobbyPlayerEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 어느 팀에 속했는지 리스트에서 확인할 수 있도록 InTeam 을 추가
	UFUNCTION(BlueprintCallable)
	void SetupLobbyPlayerEntry(const FString& InPlayerName, bool bInIsHost, bool bInIsReady, EGameTeam InTeam);

protected:
	UFUNCTION(BlueprintImplementableEvent)
	void OnLobbyPlayerInfoUpdated(const FString& InPlayerName, bool bInIsHost, bool bInIsReady, EGameTeam InTeam);

private:
	void UpdateTextBlocks();
	static FText GetTeamDisplayText(EGameTeam InTeam);

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HostText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReadyText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TeamText;

	FString PlayerName;
	bool bIsHost = false;
	bool bIsReady = false;
	EGameTeam Team = EGameTeam::None;
};
