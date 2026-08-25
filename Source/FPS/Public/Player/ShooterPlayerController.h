// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameMode/TeamTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"

#include "ShooterPlayerController.generated.h"

class URogueAbilitySystemComponent;
class ULoadingScreenSubsystem;
class ULoadingScreenWidget;
class UNetworkTimeSyncComponent;
class UShooterInputConfig;
class USteamVoiceComponent;
class APlayerState;
struct FInputActionValue;

UCLASS()
class FPS_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AShooterPlayerController();

	UFUNCTION(BlueprintCallable, Category = "Shooter|Lobby")
	void SetLobbyReady(bool bNewLobbyReady);

	UFUNCTION(BlueprintCallable, Category = "Shooter|Voice")
	void StartVoiceTalking();

	UFUNCTION(BlueprintCallable, Category = "Shooter|Voice")
	void StopVoiceTalking();

	UFUNCTION(BlueprintCallable, Category = "Shooter|Voice")
	void SubmitCompressedVoicePacket(const TArray<uint8>& CompressedVoiceData);

	UFUNCTION(BlueprintImplementableEvent, Category = "Shooter|Voice")
	void ReceiveCompressedVoicePacket(APlayerState* SpeakerPlayerState, const TArray<uint8>& CompressedVoiceData);

	// 클라이언트는 GameMode 에 직접 접근할 수 없으므로 서버 RPC 로 팀 선택을 요청
	UFUNCTION(BlueprintCallable, Category = "Shooter|Lobby")
	void SetLobbyTeam(EGameTeam RequestedTeam);

	// 콘솔(~)에서 바로 테스트할 수 있는 임시 진입점. 정식 설정 메뉴가 생기면 그 UI가
	// UShooterGameUserSettings::SetDynamicCrosshairEnabled를 직접 불러도 된다 - 이 함수는
	// 그 위에 얹은 토글 편의 기능일 뿐이다.
	UFUNCTION(Exec)
	void ToggleDynamicCrosshair();

	UFUNCTION(BlueprintCallable, Category = "Shooter|Loading")
	void ShowLoadingScreen(float InitialProgress = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Shooter|Loading")
	void SetLoadingProgress(float Progress);

	UFUNCTION(BlueprintCallable, Category = "Shooter|Loading")
	void HideLoadingScreen();

	// 호스트는 현재 방을 유지한 채 공유 로비로 돌아간다.
	// 일반 클라이언트는 서버 맵을 바꿀 수 없으므로 방을 떠난 뒤 Frontend로 돌아간다.
	UFUNCTION(BlueprintCallable, Category = "Shooter|System")
	void RequestReturnToLobby();

	// 현재 온라인 세션을 먼저 정리한 뒤 프로젝트 설정의 Game Default Map으로 돌아간다.
	UFUNCTION(BlueprintCallable, Category = "Shooter|System")
	void RequestReturnToFrontend();

	UFUNCTION(BlueprintCallable, Category = "Shooter|System")
	void RequestQuitGame();

	UFUNCTION(Client, Reliable)
	void ClientShowLoadingScreen(float InitialProgress);

	UFUNCTION(Client, Reliable)
	void ClientSetLoadingProgress(float Progress);

	UFUNCTION(Client, Reliable)
	void ClientHideLoadingScreen();

	UFUNCTION(BlueprintPure, Category = "Shooter|Network")
	UNetworkTimeSyncComponent* GetNetworkTimeSyncComponent() const { return NetworkTimeSyncComponent; }

	USteamVoiceComponent* GetSteamVoiceComponent() const { return SteamVoiceComponent; }

	// 라운드, 사망, 전체 입력 차단처럼 모든 게임플레이 입력에 공통인 게이트다.
	UFUNCTION(BlueprintPure, Category = "FPS|Input")
	virtual bool CanProcessGameplayInput() const;

	// Data Jack 설치·해체 중에는 이동과 행동만 막고 Look은 공통 게이트만 사용한다.
	UFUNCTION(BlueprintPure, Category = "FPS|Input")
	virtual bool CanProcessMovementInput() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Input")
	virtual bool CanProcessActionInput() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyLoadedWorld(FName WorldPackageName, bool bFinalDest) override;
	virtual void ReceivedPlayer() override;
	virtual void SetupInputComponent() override;

	UFUNCTION(Server, Reliable)
	void ServerSetLobbyReady(bool bNewLobbyReady);

	UFUNCTION(Server, Reliable)
	void ServerSetVoiceTalking(bool bNewVoiceTalking);

	UFUNCTION(Server, Unreliable)
	void ServerSubmitCompressedVoicePacket(const TArray<uint8>& CompressedVoiceData);

	UFUNCTION(Client, Unreliable)
	void ClientReceiveCompressedVoicePacket(APlayerState* SpeakerPlayerState, const TArray<uint8>& CompressedVoiceData);

	UFUNCTION(Server, Reliable)
	void ServerNotifyLoadedIntoMatch();

	UFUNCTION(Server, Reliable)
	void ServerSetLobbyTeam(EGameTeam RequestedTeam);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Loading")
	TSubclassOf<ULoadingScreenWidget> LoadingScreenWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input")
	TObjectPtr<UShooterInputConfig> InputConfig;

	URogueAbilitySystemComponent* GetRogueAbilitySystemComponent() const;

private:
	void StartMatchLoadReadyCheck();
	void StopMatchLoadReadyCheck();
	void CheckMatchLoadReady();
	bool HasDataJackInteractionInputBlock() const;
	ULoadingScreenSubsystem* GetLoadingScreenSubsystem() const;

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpPressed();
	void Input_JumpReleased();
	void Input_SprintPressed();
	void Input_SprintReleased();
	void Input_PushToTalkPressed();
	void Input_PushToTalkReleased();
	void Input_AbilityPressed(const FInputActionValue& Value, FGameplayTag InputTag);
	void Input_AbilityReleased(const FInputActionValue& Value, FGameplayTag InputTag);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooter|Network", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNetworkTimeSyncComponent> NetworkTimeSyncComponent;

	UPROPERTY(VisibleAnywhere, Category = "Shooter|Voice")
	TObjectPtr<USteamVoiceComponent> SteamVoiceComponent;

	FTimerHandle MatchLoadReadyCheckTimerHandle;
	bool bNotifiedLoadedIntoMatch = false;
};
