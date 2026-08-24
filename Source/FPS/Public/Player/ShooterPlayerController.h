// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameMode/TeamTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"

#include "ShooterPlayerController.generated.h"

class URogueAbilitySystemComponent;
class ULoadingScreenSubsystem;
class ULoadingScreenWidget;
class UShooterInputConfig;
struct FInputActionValue;

UCLASS()
class FPS_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AShooterPlayerController();

	UFUNCTION(BlueprintCallable, Category = "Shooter|Lobby")
	void SetLobbyReady(bool bNewLobbyReady);

	// 콘솔(~)에서 바로 테스트할 수 있는 임시 진입점. 정식 설정 메뉴가 생기면 그 UI가
	// UShooterGameUserSettings::SetDynamicCrosshairEnabled를 직접 불러도 된다 - 이 함수는
	// 그 위에 얹은 토글 편의 기능일 뿐이다.
	UFUNCTION(Exec)
	void ToggleDynamicCrosshair();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyLoadedWorld(FName WorldPackageName, bool bFinalDest) override;
	virtual void SetupInputComponent() override;

	UFUNCTION(Server, Reliable)
	void ServerSetLobbyReady(bool bNewLobbyReady);

	UFUNCTION(Server, Reliable)
	void ServerSetLobbyTeam(EGameTeam RequestedTeam);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Loading")
	TSubclassOf<ULoadingScreenWidget> LoadingScreenWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input")
	TObjectPtr<UShooterInputConfig> InputConfig;

	URogueAbilitySystemComponent* GetRogueAbilitySystemComponent() const;

private:
	bool HasDataJackInteractionInputBlock() const;
	ULoadingScreenSubsystem* GetLoadingScreenSubsystem() const;

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpPressed();
	void Input_JumpReleased();
	void Input_SprintPressed();
	void Input_SprintReleased();
	void Input_AbilityPressed(const FInputActionValue& Value, FGameplayTag InputTag);
	void Input_AbilityReleased(const FInputActionValue& Value, FGameplayTag InputTag);
};
