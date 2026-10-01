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
#include "Loading/LoadingScreenSubsystem.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/UITags.h"
#include "TimerManager.h"
#include "UI/Common/ShooterConfirmDialog.h"
#include "UI/Foundation/ShooterButtonBase.h"
#include "UI/InGame/HUD/ShooterChatInputBox.h"
#include "UI/OutGame/Room/ShooterRoomMenuScreen.h"
#include "UI/OutGame/Room/ShooterRoomPlayerSlot.h"
#include "UI/Settings/ShooterOptionsScreen.h"
#include "UI/ShooterUISettings.h"
#include "UI/Subsystem/ShooterProfileImageSubsystem.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterRoomScreen, Log, All);

namespace ShooterRoomScreenRules
{
	constexpr float ReconcileInterval = 0.25f;

	// 화면에 그리는 순서
	constexpr EGameTeam TeamOrder[] = {EGameTeam::TeamA, EGameTeam::TeamB, EGameTeam::None};

	// 팀 인원 / 레디 수
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
	Btn_ModeNext->OnClicked().AddUObject(this, &ThisClass::HandleModeNextClicked);

	// 방장은 NativeGetDesiredFocusTarget이 nullptr이라, 채팅 입력창이 Pop된 뒤 포커스가 돌아올 곳이
	// 화면 자신뿐이다. 포커스가 화면 밖으로 새면 Enter 프리뷰를 못 받아 채팅을 다시 못 연다.
	SetIsFocusable(true);
}

FReply UShooterRoomScreen::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// 대기방은 Menu 입력 모드라 Enhanced Input의 OpenChatAction이 PC까지 오지 않는다.
	// 인게임처럼 Enter로 채팅을 열 수 있게 여기서 가로챈다.
	if (InKeyEvent.GetKey() == EKeys::Enter && ChatInputBoxClass)
	{
		OpenChat();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UShooterRoomScreen::OpenChat()
{
	AShooterPlayerController* PlayerController = GetLocalShooterPlayerController();
	if (!IsValid(PlayerController) || PlayerController->IsChatInputOpen())
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UShooterUIManagerSubsystem* UIManager = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr;
	if (!IsValid(UIManager))
	{
		return;
	}

	UShooterChatInputBox* ChatInput =
		Cast<UShooterChatInputBox>(UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, ChatInputBoxClass));
	if (!IsValid(ChatInput))
	{
		return;
	}

	ChatInput->SetRenderTranslation(ChatInputScreenOffset);
	PlayerController->BeginChatInput(ChatInput);
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

	if (UShooterRoomMenuScreen* Menu = RoomMenuInstance.Get())
	{
		Menu->OnCloseRequested.RemoveAll(this);
		Menu->OnSettingsRequested.RemoveAll(this);
		Menu->OnLeaveRequested.RemoveAll(this);
	}
	RoomMenuInstance.Reset();

	if (UShooterOptionsScreen* Screen = OptionsScreenInstance.Get())
	{
		Screen->OnCloseRequested.RemoveAll(this);
	}
	OptionsScreenInstance.Reset();

	Super::NativeOnDeactivated();
}

UWidget* UShooterRoomScreen::NativeGetDesiredFocusTarget() const
{
	return IsLocalPlayerHost() ? nullptr : Btn_Ready;
}

bool UShooterRoomScreen::NativeOnHandleBackAction()
{
	PresentRoomMenu();
	return true;
}

void UShooterRoomScreen::PresentRoomMenu()
{
	if (RoomMenuInstance.IsValid())
	{
		return;
	}

	const TSubclassOf<UShooterRoomMenuScreen> LoadedClass = RoomMenuClass.LoadSynchronous();
	const UGameInstance* GameInstance = GetGameInstance();
	UShooterUIManagerSubsystem* UIManager = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr;
	if (!IsValid(UIManager) || !IsValid(LoadedClass))
	{
		UE_LOG(LogShooterRoomScreen, Error, TEXT("RoomMenuClass is not configured."));
		return;
	}

	UShooterRoomMenuScreen* Menu = Cast<UShooterRoomMenuScreen>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, LoadedClass));
	if (!IsValid(Menu))
	{
		return;
	}

	// 항복 · 게임 종료는 연결하지 않는다
	Menu->OnCloseRequested.AddUObject(this, &ThisClass::RemoveRoomMenu);
	Menu->OnSettingsRequested.AddUObject(this, &ThisClass::PresentOptionsScreen);
	Menu->OnLeaveRequested.AddUObject(this, &ThisClass::HandleLeaveClicked);
	RoomMenuInstance = Menu;
}

void UShooterRoomScreen::RemoveRoomMenu()
{
	UShooterRoomMenuScreen* Menu = RoomMenuInstance.Get();
	if (!IsValid(Menu))
	{
		return;
	}

	Menu->OnCloseRequested.RemoveAll(this);
	Menu->OnSettingsRequested.RemoveAll(this);
	Menu->OnLeaveRequested.RemoveAll(this);

	const UGameInstance* GameInstance = GetGameInstance();
	if (UShooterUIManagerSubsystem* UIManager = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr)
	{
		UIManager->RemoveWidgetFromLayer(UITags::UI_Layer_Modal, Menu);
	}

	RoomMenuInstance.Reset();
}

void UShooterRoomScreen::PresentOptionsScreen()
{
	if (OptionsScreenInstance.IsValid())
	{
		return;
	}

	TSubclassOf<UShooterOptionsScreen> LoadedClass = OptionsScreenClass.LoadSynchronous();
	if (!IsValid(LoadedClass))
	{
		LoadedClass = UShooterUISettings::LoadDefaultOptionsScreenClass();
	}
	const UGameInstance* GameInstance = GetGameInstance();
	UShooterUIManagerSubsystem* UIManager = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr;
	if (!IsValid(UIManager) || !IsValid(LoadedClass))
	{
		UE_LOG(LogShooterRoomScreen, Error, TEXT("Options screen class is unavailable. Assign OptionsScreenClass or configure the default options screen."));
		return;
	}

	UShooterOptionsScreen* Screen = Cast<UShooterOptionsScreen>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, LoadedClass));
	if (!IsValid(Screen))
	{
		return;
	}

	Screen->OnCloseRequested.AddUObject(this, &ThisClass::HandleOptionsCloseRequested);
	OptionsScreenInstance = Screen;
}

void UShooterRoomScreen::HandleOptionsCloseRequested()
{
	UShooterOptionsScreen* Screen = OptionsScreenInstance.Get();
	if (!IsValid(Screen))
	{
		return;
	}

	Screen->FinishEditing();
	Screen->OnCloseRequested.RemoveAll(this);

	const UGameInstance* GameInstance = GetGameInstance();
	if (UShooterUIManagerSubsystem* UIManager = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr)
	{
		UIManager->RemoveWidgetFromLayer(UITags::UI_Layer_Modal, Screen);
	}

	OptionsScreenInstance.Reset();
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

	const UGameInstance* GameInstance = GetGameInstance();
	UShooterProfileImageSubsystem* ProfileImages = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterProfileImageSubsystem>()
		: nullptr;

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
		// 없으면 다음 reconcile 에서 다시 물음
		View.ProfileImage = IsValid(ProfileImages)
			? ProfileImages->FindOrRequestProfileImage(PlayerState->GetUniqueId())
			: nullptr;
		Views.Add(View);
	}

	return Views;
}

FString UShooterRoomScreen::BuildPresentationSignature(
	const TArray<FShooterRoomSlotView>& Views,
	const ALobbyGameState* LobbyGameState) const
{
	FString Signature;

	for (const EGameTeam Team : ShooterRoomScreenRules::TeamOrder)
	{
		for (const FShooterRoomSlotView& View : FShooterRoomRules::GetTeamPlayers(Views, Team))
		{
			Signature += FString::Printf(TEXT("%d:%s:%d:%d:%d:%d|"),
			                             View.PlayerId,
			                             *View.PlayerName,
			                             static_cast<int32>(View.Team),
			                             View.bIsReady ? 1 : 0,
			                             View.bIsHost ? 1 : 0,
			                             View.ProfileImage ? 1 : 0);
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
	const AShooterPlayerState* LocalPlayerState = GetLocalShooterPlayerState();
	const bool bJoinable = !IsValid(LocalPlayerState) || !LocalPlayerState->IsLobbyReady();

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
			RoomSlot->SetEmpty(Team, bJoinable);
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
	// 레디한 뒤에는 팀을 바꿀 수 없음
	const AShooterPlayerState* LocalPlayerState = GetLocalShooterPlayerState();
	if (IsValid(LocalPlayerState) && LocalPlayerState->IsLobbyReady())
	{
		return;
	}

	if (AShooterPlayerController* PlayerController = GetLocalShooterPlayerController())
	{
		PlayerController->SetLobbyTeam(Team);
	}
}

void UShooterRoomScreen::RefreshModePanel(const ELobbyMatchMode SelectedMatchMode, const bool bIsHost)
{
	// 모드를 고르는 사람은 호스트뿐
	const ESlateVisibility ArrowVisibility = bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
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

	// 로딩 컨텍스트 캐시
	FShooterLoadingContext LoadingContext;
	if (Entry)
	{
		LoadingContext.ModeName = Entry->ModeName;
		LoadingContext.Thumbnail = Entry->Thumbnail;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	ULoadingScreenSubsystem* LoadingSubsystem = IsValid(GameInstance)
		? GameInstance->GetSubsystem<ULoadingScreenSubsystem>()
		: nullptr;
	if (LoadingSubsystem)
	{
		// Entry 를 못 찾으면 빈 컨텍스트가 들어가 직전 모드가 지워짐
		LoadingSubsystem->SetPendingLoadingContext(LoadingContext);
	}

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
	// 래퍼를 지정했으면 위에 얹은 장식까지 함께 사라지도록 래퍼를 숨긴다.
	UWidget* ReadyTarget = Btn_Ready;
	if (IsValid(Wrapper_Ready))
	{
		ReadyTarget = Wrapper_Ready;
	}
	ReadyTarget->SetVisibility(bIsHost ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	const FText ReadyLabel = bIsLocalReady
		? NSLOCTEXT("ShooterLobby", "CancelReady", "CANCEL")
		: NSLOCTEXT("ShooterLobby", "Ready", "READY");
	if (IsValid(Text_Ready))
	{
		Text_Ready->SetText(ReadyLabel);
	}
	else
	{
		Btn_Ready->SetButtonText(ReadyLabel);
	}

	UWidget* StartTarget = Btn_Start;
	if (IsValid(Wrapper_Start))
	{
		StartTarget = Wrapper_Start;
	}
	StartTarget->SetVisibility(bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

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
			FText::AsNumber(StartState.ReadyCount + 1),
			FText::AsNumber(StartState.ReadyRequired + 1));

	case ELobbyStartBlocker::None:
	default:
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
