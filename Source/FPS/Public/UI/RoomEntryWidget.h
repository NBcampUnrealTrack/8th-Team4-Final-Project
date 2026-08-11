// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Session/RoomInfo.h"
#include "RoomEntryWidget.generated.h"

class UButton;
class UMenuWidget;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class FPS_API URoomEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetupRoomEntry(const FRoomInfo& InRoomInfo, UMenuWidget* InOwningMenu);

	UFUNCTION(BlueprintPure)
	const FRoomInfo& GetRoomInfo() const { return RoomInfo; }

protected:
	virtual bool Initialize() override;

	UFUNCTION(BlueprintImplementableEvent)
	void OnRoomInfoUpdated(const FRoomInfo& InRoomInfo);

private:
	UFUNCTION()
	void JoinButtonClicked();

	void UpdateTextBlocks();

private:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> JoinButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RoomNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MapNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MatchTypeText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerCountText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PingText;

	UPROPERTY(Transient)
	TObjectPtr<UMenuWidget> OwningMenu;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FRoomInfo RoomInfo;
};
