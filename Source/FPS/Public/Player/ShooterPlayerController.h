// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/SurrenderVoteTypes.h"
#include "GameMode/Types/TeamTypes.h"
#include "GameMode/Types/LobbyTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "Voice/VoiceChatSubsystem.h"

#include "ShooterPlayerController.generated.h"

class URogueAbilitySystemComponent;
class URogueHealthComponent;
class ULoadingScreenSubsystem;
class ULoadingScreenWidget;
class UNetworkTimeSyncComponent;
class UShooterSpectatorWidget;
class UShooterInputConfig;
class USteamVoiceComponent;
class USurrenderVoteManager;
class UTeamMatchUIInitComponent;
class UInputMappingContext;
class APlayerState;
class USoundBase;
class USoundMix;
struct FInputActionValue;

// 크로스헤어(UShooterReticle)가 구독해서 히트마커 표시. bRed면 빨간 X(헤드샷 또는 처치), 아니면 기본 X.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHitMarker, bool /*bRed*/);
DECLARE_MULTICAST_DELEGATE(FOnSurrenderVoteStateChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSurrenderVoteStartResult, ESurrenderVoteStartResult);

UCLASS()
class FPS_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AShooterPlayerController();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 크로스헤어가 이 델리게이트를 구독해 히트마커를 표시
	FOnHitMarker OnHitMarker;

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

	// 방 화면이 부르는 입구
	UFUNCTION(BlueprintCallable, Category = "Shooter|Lobby")
	void RequestStartRoomGame();

	UFUNCTION(BlueprintCallable, Category = "Shooter|Lobby")
	void RequestLobbyMatchMode(ELobbyMatchMode RequestedMatchMode);

	UFUNCTION(Client, Reliable)
	void ClientNotifyLobbyStartFailed();

	// 방 화면이 Activated/Deactivated 쌍으로 구독
	FSimpleMulticastDelegate OnLobbyStartFailed;

	// 콘솔(~)에서 바로 테스트할 수 있는 임시 진입점. 정식 설정 메뉴가 생기면 그 UI가
	// UShooterGameUserSettings::SetDynamicCrosshairEnabled를 직접 불러도 된다 - 이 함수는
	// 그 위에 얹은 토글 편의 기능일 뿐이다.
	UFUNCTION(Exec)
	void ToggleDynamicCrosshair();

	// TEMP 디버그: 데미지 파이프라인을 거치지 않고 피격 방향 인디케이터만 강제로 발동시킨다.
	// 콘솔(~)에서 "TestHitIndicator" 입력 - 인디케이터 UI(회전/페이드)만 따로 검증할 때 쓴다.
	// 실제 데미지 파이프라인이 정상화되면 이 함수는 지워도 된다.
	UFUNCTION(Exec)
	void TestHitIndicator();

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

	const FSurrenderVoteState& GetSurrenderVoteState() const;
	void RequestStartSurrenderVote();
	void SubmitSurrenderVote(bool bApprove);

	FOnSurrenderVoteStateChanged OnSurrenderVoteStateChanged;
	FOnSurrenderVoteStartResult OnSurrenderVoteStartResult;

	UFUNCTION(Client, Reliable)
	void ClientShowLoadingScreen(float InitialProgress);

	UFUNCTION(Client, Reliable)
	void ClientSetLoadingProgress(float Progress);

	UFUNCTION(Client, Reliable)
	void ClientHideLoadingScreen();

	// 서버가 히트를 확정한 직후 공격자 본인에게만 알려 크로스헤어 히트마커를 재생.
	UFUNCTION(Client, Reliable)
	void ClientPlayHitMarker(bool bRed);

	// 서버가 킬을 확정한 직후 처치자 본인에게만 알려 킬 사운드를 재생
	UFUNCTION(Client, Reliable)
	void ClientPlayKillSound();

	UFUNCTION(Client, Reliable)
	void ClientEnterSpectatorMode(APlayerState* ViewTargetPlayerState);

	UFUNCTION(Client, Reliable)
	void ClientSetSpectatorTarget(APlayerState* ViewTargetPlayerState);

	UFUNCTION(Client, Reliable)
	void ClientExitSpectatorMode();

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

	// 공용 Controller는 모드별 GameState 타입을 모르며, 필요한 바인딩은 전용 Controller가 구현한다.
	virtual void RefreshLoadingScreenGameStateBinding(ULoadingScreenSubsystem* LoadingScreenSubsystem);
	virtual void ClearLoadingScreenGameStateBinding(ULoadingScreenSubsystem* LoadingScreenSubsystem);

	UFUNCTION(Server, Reliable)
	void ServerSetLobbyReady(bool bNewLobbyReady);

	UFUNCTION(Server, Reliable)
	void ServerRequestStartRoomGame();

	UFUNCTION(Server, Reliable)
	void ServerRequestLobbyMatchMode(ELobbyMatchMode RequestedMatchMode);

	UFUNCTION(Server, Reliable)
	void ServerSetVoiceTalking(bool bNewVoiceTalking);

	UFUNCTION(Server, Unreliable)
	void ServerSubmitCompressedVoicePacket(const TArray<uint8>& CompressedVoiceData, EVoiceChatAudience Audience);

	UFUNCTION(Client, Unreliable)
	void ClientReceiveCompressedVoicePacket(
		APlayerState* SpeakerPlayerState,
		const TArray<uint8>& CompressedVoiceData,
		EVoiceChatAudience Audience);

	UFUNCTION(Server, Reliable)
	void ServerNotifyLoadedIntoMatch();

	UFUNCTION(Server, Reliable)
	void ServerCycleSpectatorTarget();

	UFUNCTION(Server, Reliable)
	void ServerSetLobbyTeam(EGameTeam RequestedTeam);

	UFUNCTION(Server, Reliable)
	void ServerRequestStartSurrenderVote();

	UFUNCTION(Server, Reliable)
	void ServerSubmitSurrenderVote(int32 VoteId, bool bApprove);

	UFUNCTION(Client, Reliable)
	void ClientNotifySurrenderVoteStartResult(ESurrenderVoteStartResult Result);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Loading")
	TSubclassOf<ULoadingScreenWidget> LoadingScreenWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Spectator")
	TSubclassOf<UShooterSpectatorWidget> SpectatorWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input")
	TObjectPtr<UShooterInputConfig> InputConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Feedback")
	TObjectPtr<USoundBase> LocalKillSound;

	URogueAbilitySystemComponent* GetRogueAbilitySystemComponent() const;
	void SetMappingContextEnabled(
		UInputMappingContext* MappingContext,
		int32 Priority,
		bool bEnabled);

private:
	friend class USurrenderVoteManager;

	UFUNCTION()
	void OnRep_SurrenderVoteState();

	void SetSurrenderVoteState(const FSurrenderVoteState& NewState);
	void InitializeAudioSettings();
	void ShutdownAudioSettings();
	void HandleAudioSettingsChanged();
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
	void Input_CrouchPressed();
	void Input_CrouchReleased();
	void Input_PushToTalkPressed();
	void Input_PushToTalkReleased();
	void Input_ToggleInGameMenu();
	void Input_ShowScoreboard();
	void Input_HideScoreboard();
	void Input_AbilityPressed(const FInputActionValue& Value, FGameplayTag InputTag);
	void Input_AbilityReleased(const FInputActionValue& Value, FGameplayTag InputTag);
	bool IsSpectatorCycleInput(FGameplayTag InputTag) const;
	bool IsInSpectatorFlow() const;
	void ShowSpectatorWidget(APlayerState* ViewTargetPlayerState);
	void RefreshSpectatorWidget(APlayerState* ViewTargetPlayerState);
	void HideSpectatorWidget();
	void SetSystemMappingContextEnabled(bool bEnabled);
	UTeamMatchUIInitComponent* GetTeamMatchUIInitComponent() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooter|Network", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNetworkTimeSyncComponent> NetworkTimeSyncComponent;

	UPROPERTY(VisibleAnywhere, Category = "Shooter|Voice")
	TObjectPtr<USteamVoiceComponent> SteamVoiceComponent;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> ActiveUserSettingsSoundMix;

	UPROPERTY()
	TObjectPtr<UShooterSpectatorWidget> SpectatorWidget;

	UPROPERTY(ReplicatedUsing = OnRep_SurrenderVoteState)
	FSurrenderVoteState SurrenderVoteState;

	FTimerHandle MatchLoadReadyCheckTimerHandle;
	bool bNotifiedLoadedIntoMatch = false;
};
