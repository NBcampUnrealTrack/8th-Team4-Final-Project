#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "VoicePlayerEntryWidget.generated.h"

class APlayerState;
class AShooterPlayerState;
class UImage;
class UTextBlock;
class UVoiceChatSubsystem;

UCLASS(BlueprintType, Blueprintable)
class FPS_API UVoicePlayerEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Shooter|Voice")
	void SetupVoicePlayerEntry(APlayerState* InPlayerState, APlayerState* InLocalPlayerState);

	UFUNCTION(BlueprintCallable, Category = "Shooter|Voice")
	void RefreshVoicePlayerEntry();

	UFUNCTION(BlueprintCallable, Category = "Shooter|Voice")
	void TogglePlayerMute();

protected:
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Shooter|Voice")
	void OnVoicePlayerEntryUpdated(
		const FString& DisplayName,
		bool bIsLocalPlayer,
		bool bIsTalking,
		bool bIsMuted,
		bool bCanHearVoice);

private:
	UFUNCTION()
	void HandleVoiceTalkingChanged(AShooterPlayerState* ChangedPlayerState, bool bIsTalking);

	UVoiceChatSubsystem* GetVoiceChatSubsystem() const;
	FString GetDisplayName() const;
	bool IsLocalPlayerEntry() const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Shooter|Voice|Style")
	FSlateBrush DefaultCharacterBrush;

	UPROPERTY(EditDefaultsOnly, Category = "Shooter|Voice|Style")
	FSlateBrush MutedIconBrush;

	UPROPERTY(EditDefaultsOnly, Category = "Shooter|Voice|Style")
	FSlateBrush UnmutedIconBrush;

	UPROPERTY(EditDefaultsOnly, Category = "Shooter|Voice|Style")
	FLinearColor NormalEntryColor = FLinearColor(0.02f, 0.02f, 0.02f, 0.55f);

	UPROPERTY(EditDefaultsOnly, Category = "Shooter|Voice|Style")
	FLinearColor TalkingEntryColor = FLinearColor(0.0f, 0.35f, 0.25f, 0.85f);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> CharacterImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> MuteIcon;

	UPROPERTY()
	TObjectPtr<APlayerState> VoicePlayerState;

	UPROPERTY()
	TObjectPtr<APlayerState> LocalPlayerState;
};
