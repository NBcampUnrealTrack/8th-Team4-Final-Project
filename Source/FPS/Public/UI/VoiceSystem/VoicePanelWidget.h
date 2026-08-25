#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Voice/VoiceChatSubsystem.h"
#include "VoicePanelWidget.generated.h"

class APlayerState;
class UPanelWidget;
class UVoicePlayerEntryWidget;

UCLASS(BlueprintType, Blueprintable)
class FPS_API UVoicePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Shooter|Voice")
	void RefreshVoicePanel();

	UFUNCTION(BlueprintCallable, Category = "Shooter|Voice")
	void SetVoiceAudience(EVoiceChatAudience NewAudience);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnInitialized() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Shooter|Voice")
	void OnVoicePanelRefreshed(int32 PlayerCount, EVoiceChatAudience CurrentAudience);

	UFUNCTION(BlueprintImplementableEvent, Category = "Shooter|Voice")
	void OnVoiceAudienceUpdated(EVoiceChatAudience CurrentAudience);

private:
	UFUNCTION()
	void HandleMuteChanged(APlayerState* PlayerState, bool bMuted);

	void NotifyAudienceUpdated();
	APlayerState* GetLocalPlayerState() const;
	UVoiceChatSubsystem* GetVoiceChatSubsystem() const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Shooter|Voice")
	TSubclassOf<UVoicePlayerEntryWidget> EntryWidgetClass;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> PlayerListPanel;

	UPROPERTY()
	TArray<TObjectPtr<UVoicePlayerEntryWidget>> EntryWidgets;
};
