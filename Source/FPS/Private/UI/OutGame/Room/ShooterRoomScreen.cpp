// ShooterRoomScreen.cpp
#include "UI/OutGame/Room/ShooterRoomScreen.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameMode/State/LobbyGameState.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/UITags.h"
#include "TimerManager.h"
#include "UI/Common/ShooterConfirmDialog.h"
#include "UI/Foundation/ShooterButtonBase.h"
#include "UI/OutGame/Room/ShooterRoomPlayerSlot.h"
#include "UI/ShooterUISettings.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterRoomScreen, Log, All);

namespace ShooterRoomScreenRules
{
	constexpr float ReconcileInterval = 0.25f;

	// 화면에 그리는 순서
	constexpr EGameTeam TeamOrder[] = {EGameTeam::TeamA, EGameTeam::TeamB, EGameTeam::None};

	// 레디 수 / 팀 인원
	FText BuildReadyCountText(const TArray<FShooterRoomSlotView>& TeamViews)
	{
		int32 ReadyCount = 0;
		for (const FShooterRoomSlotView& View : TeamViews)
		{
			ReadyCount += View.bIsReady ? 1 : 0;
		}

		return FText::Format(
			NSLOCTEXT("ShooterLobby", "TeamReadyCount", "{0} / {1}"),
			FText::AsNumber(ReadyCount),
			FText::AsNumber(TeamViews.Num()));
	}
}

void UShooterRoomScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Btn_Ready->OnClicked().AddUObject(this, &ThisClass::HandleReadyClicked);
	Btn_Start->OnClicked().AddUObject(this, &ThisClass::HandleStartClicked);
	Btn_Leave->OnClicked().AddUObject(this, &ThisClass::HandleLeaveClicked);
	Btn_ModePrev->OnClicked().AddUObject(this, &ThisClass::HandleModePrevClicked);
	Btn_ModeNext->OnClicked().AddUObject(this, &ThisClass::HandleModeNextClicked);
}

void UShooterRoomScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	CachedPresentationSignature.Reset();
	bStartAllowedByServer = false;
	bLocalStartPending = false;

	if (AShooterPlayerController* PlayerController = GetLocalShooterPlayerController())
	{
		BoundPlayerController = PlayerController;
		PlayerController->OnLobbyStartFailed.AddUObject(this, &ThisClass::HandleLobbyStartFailed);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ReconcileTimerHandle, this, &ThisClass::Refresh, ShooterRoomScreenRules::ReconcileInterval, true);
	}

	Refresh();
	RequestThumbnails();
}

void UShooterRoomScreen::NativeOnDeactivated()
{
	if (ThumbnailHandle.IsValid() && !ThumbnailHandle->HasLoadCompleted())
	{
		ThumbnailHandle->CancelHandle();
	}
	ThumbnailHandle.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReconcileTimerHandle);
	}

	if (AShooterPlayerController* PlayerController = BoundPlayerController.Get())
	{
		PlayerController->OnLobbyStartFailed.RemoveAll(this);
	}
	BoundPlayerController.Reset();

	UnbindFromLobbyGameState();

	// 확인창이 결과 없이 끝나도 바인딩이 남지 않게. 확인창은 풀에서 재사용됨
	ClearLeaveConfirmBindings();

	Super::NativeOnDeactivated();
}

UWidget* UShooterRoomScreen::NativeGetDesiredFocusTarget() const
{
	// 호스트에게 포커스를 주면 Enter 한 번에 매치가 시작됨 방지를 위함
	return IsLocalPlayerHost() ? nullptr : Btn_Ready;
}

bool UShooterRoomScreen::NativeOnHandleBackAction()
{
	//  전환할 앞 칸이 없다
	HandleLeaveClicked();
	return true;
}

void UShooterRoomScreen::Refresh()
{
	ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState))
	{
		return;
	}

	if (BoundLobbyGameState.Get() != LobbyGameState)
	{
		BindToLobbyGameState(LobbyGameState);
	}

	// START 버튼 잠금
	RefreshLocalActions();

	// Room Rows에 Player 추가
	const TArray<FShooterRoomSlotView> Views = BuildSlotViews(LobbyGameState);
	const bool bIsHost = IsLocalPlayerHost();

	const FString Signature = BuildPresentationSignature(Views, LobbyGameState);
	if (Signature == CachedPresentationSignature)
	{
		return;
	}
	CachedPresentationSignature = Signature;

	RefreshTeamPanels(Views);
	RefreshRoomHeader(Views, LobbyGameState);
	RefreshModePanel(LobbyGameState->GetSelectedMatchMode(), bIsHost);
	RefreshActions(LobbyGameState, bIsHost);
}

void UShooterRoomScreen::BindToLobbyGameState(ALobbyGameState* NewGameState)
{
	UnbindFromLobbyGameState();

	if (!IsValid(NewGameState))
	{
		return;
	}

	BoundLobbyGameState = NewGameState;
	NewGameState->OnLobbyPlayerListChanged.AddDynamic(this, &ThisClass::HandleLobbyPlayerListChanged);
	NewGameState->OnLobbyMatchModeChanged.AddDynamic(this, &ThisClass::HandleLobbyMatchModeChanged);
}

void UShooterRoomScreen::UnbindFromLobbyGameState()
{
	if (ALobbyGameState* OldGameState = BoundLobbyGameState.Get())
	{
		OldGameState->OnLobbyPlayerListChanged.RemoveDynamic(this, &ThisClass::HandleLobbyPlayerListChanged);
		OldGameState->OnLobbyMatchModeChanged.RemoveDynamic(this, &ThisClass::HandleLobbyMatchModeChanged);
	}

	BoundLobbyGameState.Reset();
}

TArray<FShooterRoomSlotView> UShooterRoomScreen::BuildSlotViews(const ALobbyGameState* LobbyGameState) const
{
	TArray<FShooterRoomSlotView> Views;

	const APlayerState* HostState = LobbyGameState->GetHostPlayerState();
	const APlayerState* LocalState = GetOwningPlayerState();

	for (APlayerState* PlayerState : LobbyGameState->PlayerArray)
	{
		// 서버는 256 부터 줌
		if (!IsValid(PlayerState) || PlayerState->GetPlayerId() <= 0)
		{
			continue;
		}

		const AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		if (!IsValid(ShooterPlayerState))
		{
			continue;
		}

		FShooterRoomSlotView View;
		View.PlayerId = PlayerState->GetPlayerId();
		View.PlayerName = PlayerState->GetPlayerName();
		View.Team = ShooterPlayerState->GetTeam();
		View.bIsHost = (PlayerState == HostState);
		// 호스트 칸은 항상 채워진 상태이므로 서버도 레디 검사에서 호스트를 뺀다
		View.bIsReady = View.bIsHost || ShooterPlayerState->IsLobbyReady();
		View.bIsLocal = (PlayerState == LocalState);
		Views.Add(View);
	}

	return Views;
}

FString UShooterRoomScreen::BuildPresentationSignature(
	const TArray<FShooterRoomSlotView>& Views,
	const ALobbyGameState* LobbyGameState) const
{
	FString Signature;

	// 그리는 순서 그대로 쌓는다. PlayerArray 순서가 머신마다 달라도 같은 값이 나온다
	for (const EGameTeam Team : ShooterRoomScreenRules::TeamOrder)
	{
		for (const FShooterRoomSlotView& View : FShooterRoomRules::GetTeamPlayers(Views, Team))
		{
			Signature += FString::Printf(TEXT("%d:%s:%d:%d:%d|"),
			                             View.PlayerId,
			                             *View.PlayerName,
			                             static_cast<int32>(View.Team),
			                             View.bIsReady ? 1 : 0,
			                             View.bIsHost ? 1 : 0);
		}
	}

	const FLobbyRoomSettings& RoomSettings = LobbyGameState->GetRoomSettings();
	const FLobbyStartState& StartState = LobbyGameState->GetStartState();

	Signature += FString::Printf(TEXT("#%d#%s#%d#%d#%d#%d"),
	                             static_cast<int32>(LobbyGameState->GetSelectedMatchMode()),
	                             *RoomSettings.RoomName,
	                             RoomSettings.MaxPlayers,
	                             static_cast<int32>(StartState.Blocker),
	                             StartState.ReadyCount,
	                             StartState.ReadyRequired);

	return Signature;
}

void UShooterRoomScreen::RefreshTeamPanels(const TArray<FShooterRoomSlotView>& Views)
{
	const ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState))
	{
		return;
	}

	const int32 TeamCapacity = LobbyGameState->GetRoomSettings().MaxPlayersPerTeam;
	if (TeamCapacity <= 0)
	{
		return; // 방 설정이 아직 안 옴. 다음 reconcile
	}

	if (!ensureMsgf(
		PlayerSlotWidgetClass,
		TEXT("PlayerSlotWidgetClass must be configured on %s."),
		*GetName()))
	{
		return;
	}

	EnsureTeamSlots(Box_TeamA, TeamSlotsA, EGameTeam::TeamA, TeamCapacity);
	EnsureTeamSlots(Box_TeamB, TeamSlotsB, EGameTeam::TeamB, TeamCapacity);

	ApplyTeamSlots(TeamSlotsA, FShooterRoomRules::GetTeamPlayers(Views, EGameTeam::TeamA), EGameTeam::TeamA);
	ApplyTeamSlots(TeamSlotsB, FShooterRoomRules::GetTeamPlayers(Views, EGameTeam::TeamB), EGameTeam::TeamB);

	RebuildUnassignedEntries(FShooterRoomRules::GetTeamPlayers(Views, EGameTeam::None));
	RefreshTeamHeaders(Views);
}

void UShooterRoomScreen::EnsureTeamSlots(
	UPanelWidget* Container,
	TArray<TObjectPtr<UShooterRoomPlayerSlot>>& OutSlots,
	const EGameTeam Team,
	const int32 Capacity)
{
	if (OutSlots.Num() == Capacity)
	{
		return;
	}

	Container->ClearChildren();
	OutSlots.Reset();

	for (int32 Index = 0; Index < Capacity; ++Index)
	{
		UShooterRoomPlayerSlot* RoomSlot = CreateWidget<UShooterRoomPlayerSlot>(this, PlayerSlotWidgetClass);
		if (!IsValid(RoomSlot))
		{
			continue;
		}

		RoomSlot->OnJoinRequested.AddUObject(this, &ThisClass::HandleSlotJoinRequested);
		RoomSlot->SetEmpty(Team);
		Container->AddChild(RoomSlot);
		OutSlots.Add(RoomSlot);
	}
}

void UShooterRoomScreen::ApplyTeamSlots(
	const TArray<TObjectPtr<UShooterRoomPlayerSlot>>& Slots,
	const TArray<FShooterRoomSlotView>& TeamViews,
	const EGameTeam Team)
{
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		UShooterRoomPlayerSlot* RoomSlot = Slots[Index];
		if (!IsValid(RoomSlot))
		{
			continue;
		}

		// 칸보다 사람이 많으면 넘치는 뷰는 그리지 않음. 정원은 서버가 막음
		if (TeamViews.IsValidIndex(Index))
		{
			RoomSlot->SetPlayer(TeamViews[Index]);
		}
		else
		{
			RoomSlot->SetEmpty(Team);
		}
	}
}

void UShooterRoomScreen::RebuildUnassignedEntries(const TArray<FShooterRoomSlotView>& UnassignedViews)
{
	// 연출이 없어 매번 다시 만들어도 됨
	Box_Unassigned->ClearChildren();

	if (!ensureMsgf(
		UnassignedEntryWidgetClass,
		TEXT("UnassignedEntryWidgetClass must be configured on %s."),
		*GetName()))
	{
		return;
	}

	for (const FShooterRoomSlotView& View : UnassignedViews)
	{
		UShooterRoomPlayerSlot* Entry = CreateWidget<UShooterRoomPlayerSlot>(this, UnassignedEntryWidgetClass);
		if (!IsValid(Entry))
		{
			continue;
		}

		Entry->SetPlayer(View);
		Box_Unassigned->AddChild(Entry);
	}
}

void UShooterRoomScreen::RefreshTeamHeaders(const TArray<FShooterRoomSlotView>& Views)
{
	Text_TeamAReady->SetText(
		ShooterRoomScreenRules::BuildReadyCountText(FShooterRoomRules::GetTeamPlayers(Views, EGameTeam::TeamA)));
	Text_TeamBReady->SetText(
		ShooterRoomScreenRules::BuildReadyCountText(FShooterRoomRules::GetTeamPlayers(Views, EGameTeam::TeamB)));

	Text_UnassignedCount->SetText(
		FText::AsNumber(FShooterRoomRules::GetTeamPlayers(Views, EGameTeam::None).Num()));
}

void UShooterRoomScreen::HandleSlotJoinRequested(const EGameTeam Team)
{
	if (AShooterPlayerController* PlayerController = GetLocalShooterPlayerController())
	{
		// 정원 초과·같은 팀은 서버가 판단함
		PlayerController->SetLobbyTeam(Team);
	}
}

void UShooterRoomScreen::RefreshModePanel(const ELobbyMatchMode SelectedMatchMode, const bool bIsHost)
{
	// 모드를 고르는 사람은 호스트뿐
	const ESlateVisibility ArrowVisibility = bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	Btn_ModePrev->SetVisibility(ArrowVisibility);
	Btn_ModeNext->SetVisibility(ArrowVisibility);

	const ALobbyGameState* LobbyGameState = GetLobbyGameState();
	const int32 TeamCapacity = IsValid(LobbyGameState) ? LobbyGameState->GetRoomSettings().MaxPlayersPerTeam : 0;
	Text_TeamSize->SetVisibility(TeamCapacity > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	Text_TeamSize->SetText(FText::Format(
		NSLOCTEXT("ShooterLobby", "TeamSize", "{0}V{0}"),
		FText::AsNumber(TeamCapacity)));

	const TArray<FShooterLobbyModeDisplay>& Modes = GetDefault<UShooterUISettings>()->LobbyModes;
	const FShooterLobbyModeDisplay* Entry = Modes.FindByPredicate(
		[SelectedMatchMode](const FShooterLobbyModeDisplay& It) { return It.Mode == SelectedMatchMode; });

	if (!Entry)
	{
		Text_ModeName->SetText(UEnum::GetDisplayValueAsText(SelectedMatchMode));
		Text_MapName->SetVisibility(ESlateVisibility::Collapsed);
		Img_MapThumbnail->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Text_ModeName->SetText(Entry->ModeName);
	Text_MapName->SetText(Entry->MapName);
	Text_MapName->SetVisibility(ESlateVisibility::HitTestInvisible);

	UTexture2D* Thumbnail = Entry->Thumbnail.Get();
	Img_MapThumbnail->SetVisibility(Thumbnail ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Thumbnail)
	{
		Img_MapThumbnail->SetBrushFromTexture(Thumbnail);
	}
}

void UShooterRoomScreen::RefreshActions(const ALobbyGameState* LobbyGameState, const bool bIsHost)
{
	const AShooterPlayerState* LocalPlayerState = GetLocalShooterPlayerState();
	const bool bIsLocalReady = IsValid(LocalPlayerState) && LocalPlayerState->IsLobbyReady();

	// 호스트는 레디 대상이 아님
	Btn_Ready->SetVisibility(bIsHost ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	Btn_Ready->SetButtonText(bIsLocalReady
		                         ? NSLOCTEXT("ShooterLobby", "CancelReady", "CANCEL")
		                         : NSLOCTEXT("ShooterLobby", "Ready", "READY"));

	Btn_Start->SetVisibility(bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	Text_Status->SetText(BuildStatusText(LobbyGameState->GetStartState(), bIsHost));

	bStartAllowedByServer = (LobbyGameState->GetStartState().Blocker == ELobbyStartBlocker::None);
	RefreshLocalActions();
}

FText UShooterRoomScreen::BuildStatusText(const FLobbyStartState& StartState, const bool bIsHost) const
{
	switch (StartState.Blocker)
	{
	case ELobbyStartBlocker::Starting:
		return NSLOCTEXT("ShooterLobby", "StartingGame", "게임 시작 중");

	case ELobbyStartBlocker::NotEnoughPlayers:
	{
		const ALobbyGameState* LobbyGameState = GetLobbyGameState();
		return FText::Format(
			NSLOCTEXT("ShooterLobby", "NeedMorePlayers", "최소 {0}명이 필요합니다"),
			FText::AsNumber(IsValid(LobbyGameState) ? LobbyGameState->GetRoomSettings().MinimumPlayersToStart : 0));
	}

	case ELobbyStartBlocker::TeamsCannotBeFinalized:
		return NSLOCTEXT("ShooterLobby", "BalanceTeams", "팀 인원을 맞춰 주세요");

	case ELobbyStartBlocker::WaitingForReady:
		return FText::Format(
			NSLOCTEXT("ShooterLobby", "ReadyCount", "READY {0} / {1}"),
			FText::AsNumber(StartState.ReadyCount),
			FText::AsNumber(StartState.ReadyRequired));

	case ELobbyStartBlocker::None:
	default:
		// 같은 사실은 같은 문구. 시작할 수 있을 때만 호스트와 클라가 갈림
		return bIsHost
			? NSLOCTEXT("ShooterLobby", "CanStart", "시작할 수 있습니다")
			: NSLOCTEXT("ShooterLobby", "WaitingForHost", "호스트가 시작하기를 기다리는 중");
	}
}

void UShooterRoomScreen::RefreshRoomHeader(const TArray<FShooterRoomSlotView>& Views, const ALobbyGameState* LobbyGameState)
{
	const FLobbyRoomSettings& RoomSettings = LobbyGameState->GetRoomSettings();
	Text_RoomTitle->SetText(FText::FromString(RoomSettings.RoomName));

	Text_PlayerCount->SetText(FText::Format(
		NSLOCTEXT("ShooterLobby", "RoomPlayerCountHeader", "{0} / {1}"),
		FText::AsNumber(Views.Num()),
		FText::AsNumber(RoomSettings.MaxPlayers)));
}

void UShooterRoomScreen::RefreshLocalActions()
{
	Btn_Start->SetIsEnabled(IsLocalPlayerHost() && bStartAllowedByServer && !bLocalStartPending);
}

void UShooterRoomScreen::HandleReadyClicked()
{
	AShooterPlayerController* PlayerController = GetLocalShooterPlayerController();
	const AShooterPlayerState* PlayerState = GetLocalShooterPlayerState();
	if (!IsValid(PlayerController) || !IsValid(PlayerState))
	{
		return;
	}

	PlayerController->SetLobbyReady(!PlayerState->IsLobbyReady());
}

void UShooterRoomScreen::HandleStartClicked()
{
	AShooterPlayerController* PlayerController = GetLocalShooterPlayerController();
	if (!IsValid(PlayerController) || bLocalStartPending)
	{
		return;
	}

	bLocalStartPending = true;
	Btn_Start->SetIsEnabled(false);
	PlayerController->RequestStartRoomGame();
}

void UShooterRoomScreen::HandleModePrevClicked()
{
	StepMatchMode(-1);
}

void UShooterRoomScreen::HandleModeNextClicked()
{
	StepMatchMode(+1);
}

void UShooterRoomScreen::StepMatchMode(const int32 Delta)
{
	const ALobbyGameState* LobbyGameState = GetLobbyGameState();
	AShooterPlayerController* PlayerController = GetLocalShooterPlayerController();
	if (!IsValid(LobbyGameState) || !IsValid(PlayerController))
	{
		return;
	}

	const TArray<FShooterLobbyModeDisplay>& Modes = GetDefault<UShooterUISettings>()->LobbyModes;
	if (Modes.IsEmpty())
	{
		return;
	}

	const ELobbyMatchMode Current = LobbyGameState->GetSelectedMatchMode();
	int32 Index = Modes.IndexOfByPredicate(
		[Current](const FShooterLobbyModeDisplay& Entry) { return Entry.Mode == Current; });
	if (Index == INDEX_NONE)
	{
		Index = 0;
	}

	const int32 NextIndex = (Index + Delta + Modes.Num()) % Modes.Num();

	PlayerController->RequestLobbyMatchMode(Modes[NextIndex].Mode);
}

void UShooterRoomScreen::RequestThumbnails()
{
	TArray<FSoftObjectPath> Paths;
	for (const FShooterLobbyModeDisplay& Entry : GetDefault<UShooterUISettings>()->LobbyModes)
	{
		if (!Entry.Thumbnail.IsNull())
		{
			Paths.Add(Entry.Thumbnail.ToSoftObjectPath());
		}
	}

	if (Paths.IsEmpty())
	{
		return;
	}

	ThumbnailHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Paths,
		FStreamableDelegate::CreateWeakLambda(this, [this]()
		{
			// 요청할 때의 모드가 아니라 지금 선택된 모드로 다시 그림
			if (const ALobbyGameState* LobbyGameState = GetLobbyGameState())
			{
				RefreshModePanel(LobbyGameState->GetSelectedMatchMode(), IsLocalPlayerHost());
			}
		}));
}

void UShooterRoomScreen::HandleLeaveClicked()
{
	if (!IsLocalPlayerHost())
	{
		LeaveToFrontend();
		return;
	}

	if (LeaveConfirmDialog.IsValid())
	{
		return;
	}

	const TSubclassOf<UShooterConfirmDialog> LoadedClass = ConfirmDialogClass.LoadSynchronous();
	const UGameInstance* GameInstance = GetGameInstance();
	UShooterUIManagerSubsystem* UIManager = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr;
	if (!IsValid(UIManager) || !IsValid(LoadedClass))
	{
		UE_LOG(LogShooterRoomScreen, Error, TEXT("ConfirmDialogClass is not configured."));
		return;
	}

	UShooterConfirmDialog* Dialog = Cast<UShooterConfirmDialog>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, LoadedClass));
	if (!IsValid(Dialog))
	{
		return;
	}

	Dialog->SetupMessage(NSLOCTEXT("ShooterLobby", "LeaveRoomHost", "방이 닫히고 모두 나가게 됩니다"));
	Dialog->OnConfirmed.AddDynamic(this, &ThisClass::HandleLeaveConfirmed);
	Dialog->OnCanceled.AddDynamic(this, &ThisClass::HandleLeaveCanceled);
	LeaveConfirmDialog = Dialog;
}

void UShooterRoomScreen::HandleLeaveConfirmed()
{
	ClearLeaveConfirmBindings();
	LeaveToFrontend();
}

void UShooterRoomScreen::HandleLeaveCanceled()
{
	ClearLeaveConfirmBindings();
}

void UShooterRoomScreen::ClearLeaveConfirmBindings()
{
	if (UShooterConfirmDialog* Dialog = LeaveConfirmDialog.Get())
	{
		Dialog->OnConfirmed.RemoveAll(this);
		Dialog->OnCanceled.RemoveAll(this);
	}
	LeaveConfirmDialog.Reset();
}

void UShooterRoomScreen::LeaveToFrontend()
{
	if (AShooterPlayerController* PlayerController = GetLocalShooterPlayerController())
	{
		PlayerController->RequestReturnToFrontend();
	}
}

void UShooterRoomScreen::HandleLobbyPlayerListChanged()
{
	Refresh();
}

void UShooterRoomScreen::HandleLobbyMatchModeChanged(ELobbyMatchMode SelectedMatchMode)
{
	Refresh();
}

void UShooterRoomScreen::HandleLobbyStartFailed()
{
	bLocalStartPending = false;

	Text_Status->SetText(NSLOCTEXT("ShooterLobby", "StartFailed", "게임을 시작하지 못했습니다"));
	RefreshLocalActions();
}

ALobbyGameState* UShooterRoomScreen::GetLobbyGameState() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetGameState<ALobbyGameState>() : nullptr;
}

AShooterPlayerController* UShooterRoomScreen::GetLocalShooterPlayerController() const
{
	return Cast<AShooterPlayerController>(GetOwningPlayer());
}

AShooterPlayerState* UShooterRoomScreen::GetLocalShooterPlayerState() const
{
	return Cast<AShooterPlayerState>(GetOwningPlayerState());
}

bool UShooterRoomScreen::IsLocalPlayerHost() const
{
	const ALobbyGameState* LobbyGameState = GetLobbyGameState();
	const APlayerState* LocalState = GetOwningPlayerState();

	// GetAuthGameMode 로 판단하면 리슨 서버에서만 우연히 맞을 수 있음
	return IsValid(LobbyGameState)
		&& IsValid(LocalState)
		&& LobbyGameState->GetHostPlayerState() == LocalState;
}
