#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LobbyPlayerEntryWidget.generated.h"

class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class FPS_API ULobbyPlayerEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetupLobbyPlayerEntry(const FString& InPlayerName, bool bInIsHost, bool bInIsReady);

protected:
	UFUNCTION(BlueprintImplementableEvent)
	void OnLobbyPlayerInfoUpdated(const FString& InPlayerName, bool bInIsHost, bool bInIsReady);

private:
	void UpdateTextBlocks();

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HostText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReadyText;

	FString PlayerName;
	bool bIsHost = false;
	bool bIsReady = false;
};
