// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Chat/ChatTypes.h"
#include "GameMode/Types/SurrenderVoteTypes.h"
#include "GameMode/Types/TeamTypes.h"
#include "GameMode/Types/LobbyTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "GAS/Damage/RogueDamageTypes.h"
#include "Ping/PingTypes.h"
#include "TimerManager.h"
#include "Customization/CustomizationProfile.h"
#include "Voice/VoiceChatSubsystem.h"

#include "ShooterPlayerController.generated.h"

class URogueAbilitySystemComponent;
class URogueHealthComponent;
class ULoadingScreenSubsystem;
class ULoadingScreenWidget;
class UNetworkTimeSyncComponent;
class UShooterChatInputBox;
class UShooterSpectatorWidget;
class UShooterInputConfig;
class UShooterInteractionComponent;
class USteamVoiceComponent;
class USurrenderVoteManager;
class UTeamMatchUIInitComponent;
class UInputMappingContext;
class APlayerState;
class USoundMix;
struct FInputActionValue;

// 채팅 로그(UShooterChatLog)가 구독해서 새 메시지를 표시.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChatMessageReceivedSignature, const FChatMessage&, Message);

// 핑 인디케이터 레이어가 구독해서 월드에 마커를 띄운다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPingReceivedSignature, const FPingEvent&, Ping);

// 크로스헤어(UShooterReticle)가 구독해서 히트마커 표시. Type으로 부위별(바디/쉴드/헤드샷) 색상을,
// bKill로 처치 강조(빨간색)를 결정한다.
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHitMarker, ERogueHitMarkerType /*Type*/, bool /*bKill*/);
// 크로스헤어(UShooterReticle)가 구독해서 중앙 킬 이펙트(해골 등)를 재생. ClientPlayKillSound와 같은 시점에
// 발신되며, ShooterGameMode가 자살/팀킬을 걸러낸 뒤의 확정된 처치에만 발신하므로 OnHitMarker의 bKill보다
// 이 이벤트가 킬 이펙트 트리거로 더 정확하다.
DECLARE_MULTICAST_DELEGATE(FOnKillEffect);
DECLARE_MULTICAST_DELEGATE(FOnSurrenderVoteStateChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFullMapOpenChanged, bool /*bOpen*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFullMapZoomInput, float /*WheelDelta*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSurrenderVoteStartResult, ESurrenderVoteStartResult);

UCLASS()
class FPS_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AShooterPlayerController();
    void SubmitSavedAppearances();
    virtual void AcknowledgePossession(APawn* NewPawn) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 크로스헤어가 이 델리게이트를 구독해 히트마커를 표시
	FOnHitMarker OnHitMarker;

	// 크로스헤어가 이 델리게이트를 구독해 중앙 킬 이펙트를 표시
	FOnKillEffect OnKillEffect;

	UFUNCTION(BlueprintCallable, Category = "Shooter|Lobby")
	void SetLobbyReady(bool bNewLobbyReady);

	UFUNCTION(BlueprintCallable, Category = "Shooter|Voice")
	void StartVoiceTalking();

	UFUNCTION(BlueprintCallable, Category = "Shooter|Voice")
	void StopVoiceTalking();

	UFUNCTION(BlueprintCallable, Category = "Shooter|Voice")
	void SubmitCompressedVoicePacket(const TArray<uint8>& CompressedVoiceData);

	// Game modes can disable voice chat without changing the user's global voice-chat setting.
	virtual bool SupportsVoiceChat() const { return true; }

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

	// 정상 완료. 기존 HideLoadingScreen() 은 즉시 경로로 그대로 남는다
	void FinishLoadingScreen();

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

	UFUNCTION(Client, Reliable)
	void ClientFinishLoadingScreen();

	// 서버가 히트를 확정한 직후 공격자 본인에게만 알려 크로스헤어 히트마커(시각+효과음)를 재생.
	UFUNCTION(Client, Reliable)
	void ClientPlayHitMarker(ERogueHitMarkerType Type, bool bKill, bool bPlaySound);

	// 서버가 킬을 확정한 직후 처치자 본인에게만 알려 킬 사운드 + 중앙 킬 이펙트(OnKillEffect)를 재생.
	// KillStreak: 이번 킬이 몇 연속킬인지(1=일반 킬, 2=더블킬, 3=트리플킬, 4 이상=쿼드라킬).
	UFUNCTION(Client, Reliable)
	void ClientPlayKillSound(int32 KillStreak);

	UFUNCTION(Client, Reliable)
	void ClientEnterSpectatorMode(APlayerState* ViewTargetPlayerState);

	UFUNCTION(Client, Reliable)
	void ClientSetSpectatorTarget(APlayerState* ViewTargetPlayerState);

	UFUNCTION(Client, Reliable)
	void ClientExitSpectatorMode();

	virtual void SetViewTarget(
		AActor* NewViewTarget,
		FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams()) override;

	void SetKillcamViewTargetSuppressed(bool bSuppressed);
	virtual void UpdateHiddenComponents(
		const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponents) override;

	bool IsKillcamViewTargetSuppressed() const { return bKillcamViewTargetSuppressed; }

	void ApplyKillcamViewTarget(AActor* KillcamCamera);

	AActor* ConsumePendingViewTarget();

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

	// 채팅 로그(UShooterChatLog)가 구독해서 새 메시지를 표시.
	UPROPERTY(BlueprintAssignable, Category = "Shooter|Chat")
	FOnChatMessageReceivedSignature OnChatMessageReceived;

	UFUNCTION(BlueprintPure, Category = "Shooter|Chat")
	bool IsChatInputOpen() const { return bIsChatInputOpen; }

	// 채팅 입력창이 Enter(텍스트 있음)를 감지하면 호출한다. 로컬 검증 후 서버로 전송하고 입력창을 닫는다.
	void HandleChatSubmit(const FString& RawText);

	// 채팅 입력창이 Enter(빈 텍스트) 또는 Esc를 감지하면 호출한다. 전송 없이 입력창만 닫는다.
	void HandleChatCancel();

	// 채팅 입력창이 Tab을 감지하면 호출한다. 입력 중이던 텍스트는 유지한 채 채널만 토글한다.
	void HandleChatChannelSwitchRequested();

	// 귓속말 자동완성에서 이름을 확정하는 즉시(전송 전) 호출한다. 채널을 바로 Whisper로 잠그고
	// 최근 귓말 큐를 갱신해, 그 뒤로는 "/w 이름" 없이 메시지만 쳐도 이 사람에게 간다.
	void RegisterWhisperTarget(const FString& PlayerName);

	// 핑 인디케이터 레이어(UPingIndicatorLayer)가 구독해서 마커를 띄운다.
	UPROPERTY(BlueprintAssignable, Category = "Shooter|Ping")
	FOnPingReceivedSignature OnPingReceived;

	// 전체 맵 열림 상태. M으로 토글하고, 열려 있을 때 ESC는 설정 메뉴 대신 전체 맵만 닫는다.
	// 전체 맵 위젯(UShooterMapWidget, FullMap 모드)이 OnFullMapOpenChanged를 구독해서 보이고 숨는다.
	bool IsFullMapOpen() const { return bFullMapOpen; }
	void SetFullMapOpen(bool bOpen);

	FOnFullMapOpenChanged OnFullMapOpenChanged;

	// 전체 맵이 열려 있는 동안의 마우스 휠 입력(양수 = 휠 위 = 확대). 전체 맵 위젯이 구독한다.
	FOnFullMapZoomInput OnFullMapZoomInput;

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

	// 레이트리밋/길이 재검증 후 GameState::ForEachChatRecipient로 팀/전체 필터링해 전달한다.
	UFUNCTION(Server, Reliable)
	void ServerSendChatMessage(const FString& MessageText, EChatChannel Channel);

	// "/w 닉네임 메시지" 명령으로 전송. GameState::FindChatRecipientByName으로 대상을 찾아
	// 그 사람에게만 전달하고, 보낸 사람에게는 "-> 닉네임" 형태로 에코해 자기가 보낸 걸 확인시켜준다.
	UFUNCTION(Server, Reliable)
	void ServerSendWhisperMessage(const FString& TargetName, const FString& MessageText);

	UFUNCTION(Client, Reliable)
	void ClientReceiveChatMessage(const FChatMessage& Message);

	// 레이트리밋 재검증 후 GameState::ForEachChatRecipient(Team)로 같은 팀에게만 전달한다.
	// 핑도 팀 전용이라 채팅의 팀 라우팅 로직을 그대로 재사용한다.
	UFUNCTION(Server, Reliable)
	void ServerSendPing(FVector_NetQuantize Location, EShooterPingType Type);

	UFUNCTION(Client, Reliable)
	void ClientReceivePing(const FPingEvent& Ping);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Loading")
	TSubclassOf<ULoadingScreenWidget> LoadingScreenWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Spectator")
	TSubclassOf<UShooterSpectatorWidget> SpectatorWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input")
	TObjectPtr<UShooterInputConfig> InputConfig;

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

	void Input_InteractPressed(const FInputActionValue& Value);
	void Input_InteractReleased(const FInputActionValue& Value);
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_AdjustADSZoom(const FInputActionValue& Value);
	void Input_JumpPressed();
	void Input_JumpReleased();
	void Input_SprintPressed();
	void Input_SprintReleased();
	void Input_CrouchPressed();
	void Input_CrouchReleased();
	void Input_PushToTalkPressed();
	void Input_PushToTalkReleased();
	void Input_OpenChat();
	void CloseChatInput();
	void Input_Ping();
	void Input_ToggleFullMap();
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooter|Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UShooterInteractionComponent> InteractionComponent;

	FGameplayTag ActiveInteractionFallbackTag;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> ActiveUserSettingsSoundMix;

	UPROPERTY()
	TObjectPtr<UShooterSpectatorWidget> SpectatorWidget;

	UPROPERTY(ReplicatedUsing = OnRep_SurrenderVoteState)
	FSurrenderVoteState SurrenderVoteState;

	FTimerHandle MatchLoadReadyCheckTimerHandle;
	bool bNotifiedLoadedIntoMatch = false;

	TWeakObjectPtr<UShooterChatInputBox> ChatInputWidget;
	bool bIsChatInputOpen = false;
	bool bFullMapOpen = false;
	EChatChannel CurrentChatChannel = EChatChannel::Team;

	// CurrentChatChannel==Whisper일 때 대상 이름. 그 외엔 비어있다.
	FString CurrentWhisperTarget;

	// 최근 귓속말 상대 큐. 앞이 최신. 최대 2명 - Tab 채널 토글에 Team/Match 다음으로 순서대로 들어간다.
	// 보내거나(HandleChatSubmit) 받으면(ClientReceiveChatMessage) 갱신된다.
	TArray<FString> RecentWhisperTargets;

	// "/w 닉네임 메시지" 형태면 닉네임/메시지로 분리해 true, 아니면 false.
	static bool TryParseWhisperCommand(const FString& Text, FString& OutTargetName, FString& OutMessage);

	// 서버 전용 레이트리밋 상태. 리플리케이트하지 않는다.
	double LastChatMessageServerTime = -1.0;
	double LastPingServerTime = -1.0;
	bool bKillcamViewTargetSuppressed = false;
    UFUNCTION(Server, Reliable)
    void ServerSubmitAppearance(FName ClassId, const FCustomizationAppearance& Appearance);
    FTimerHandle AppearanceSubmitTimer;
    int32 AppearanceSubmitAttempts = 0;
    double AppearanceRateWindow = -1.0;
    int32 AppearanceRateCount = 0;
    bool bAppearanceSubmitted = false;
	TWeakObjectPtr<AActor> PendingViewTarget;
	FViewTargetTransitionParams PendingViewTargetTransitionParams;
};
