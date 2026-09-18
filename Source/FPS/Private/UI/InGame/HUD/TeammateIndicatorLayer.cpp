// TeammateIndicatorLayer.cpp
#include "UI/InGame/HUD/TeammateIndicatorLayer.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameMode/State/BaseRoundGameState.h"
#include "GameMode/Types/RoundTypes.h"
#include "Player/ShooterPlayerState.h"
#include "UI/InGame/HUD/TeammateIndicatorEntry.h"
#include "Player/BaseTeamPlayerState.h"
#include "Actor/DataJack.h"
#include "GameMode/State/BombGameState.h"
#include "TimerManager.h"

namespace TeammateIndicatorLayerRules
{
	constexpr float ReconcileInterval = 0.25f;
}

void UTeammateIndicatorLayer::NativeConstruct()
{
	Super::NativeConstruct();
	
	BindToGameState();
	
	// 위젯이 라운드 시작 뒤에 만들어졌을 수 있으므로 즉시 1회 시도
	// 팀이 None이면 빠져나가고, HandleLocalTeamChanged가 나중에 다시 부른다.
	RebuildRoster();
	
	// 합류·이탈·팀 복제 지연은 이벤트로 오지 않아 주기 갱신이 필요하다
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ReconcileTimerHandle,
			this,
			&ThisClass::RebuildRoster,
			TeammateIndicatorLayerRules::ReconcileInterval,
			true);
	}
}

void UTeammateIndicatorLayer::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReconcileTimerHandle);
	}
	
	UnbindFromGameState();
	
	Super::NativeDestruct();
}

void UTeammateIndicatorLayer::BindToGameState()
{
	if (ABaseRoundGameState* GS = GetRoundGameState())
	{
		BoundGameState = GS;
		GS->OnRoundStateChanged.AddUniqueDynamic(this, &ThisClass::HandleRoundStateChanged);
	}
	
	if (ABaseTeamPlayerState* LocalPS = GetOwningPlayerState<ABaseTeamPlayerState>())
	{
		BoundLocalPlayerState = LocalPS;
		LocalPS->OnPlayerTeamChanged.AddUniqueDynamic(this, &ThisClass::HandleLocalTeamChanged);
	}
	
	// Bomb 상태 구독
	if (ABombGameState* BombGS = GetWorld() ? GetWorld()->GetGameState<ABombGameState>() : nullptr)
	{
		BoundBombGameState = BombGS;
		BombGS->OnActiveDataJackChanged.AddUniqueDynamic(this, &ThisClass::HandleActiveDataJackChanged);
		
		HandleActiveDataJackChanged(BombGS->GetActiveDataJack());
	}
}

void UTeammateIndicatorLayer::UnbindFromGameState()
{
	if (BoundGameState.IsValid())
	{
		BoundGameState->OnRoundStateChanged.RemoveAll(this);
	}
	BoundGameState.Reset();
	
	if (BoundLocalPlayerState.IsValid())
	{
		BoundLocalPlayerState->OnPlayerTeamChanged.RemoveAll(this);
	}
	BoundLocalPlayerState.Reset();
	
	if (BoundDataJack.IsValid())
	{
		BoundDataJack->OnDataJackChanged.RemoveAll(this);
	}
	BoundDataJack.Reset();
	
	if (BoundBombGameState.IsValid())
	{
		BoundBombGameState->OnActiveDataJackChanged.RemoveAll(this);
	}
	BoundBombGameState.Reset();
}

void UTeammateIndicatorLayer::HandleRoundStateChanged()
{
	const ABaseRoundGameState* GS = GetRoundGameState();
	if (GS == nullptr)
	{
		return;
	}
	
	if (GS->GetGameFlowPhase() != EGameFlowPhase::RoundStarting)
	{
		return;
	}
	
	RebuildRoster();
}

void UTeammateIndicatorLayer::HandleLocalTeamChanged(EGameTeam previousTeam, EGameTeam NewTeam)
{
	if (NewTeam == EGameTeam::None)
	{
		return;
	}
	
	RebuildRoster();
}

void UTeammateIndicatorLayer::HandleActiveDataJackChanged(ADataJack* NewActiveDataJack)
{
	if (BoundDataJack.Get() == NewActiveDataJack)
	{
		return;
	}
	
	if (BoundDataJack.IsValid())
	{
		BoundDataJack->OnDataJackChanged.RemoveAll(this);
	}
	
	BoundDataJack = NewActiveDataJack;
	
	if (IsValid(NewActiveDataJack))
	{
		NewActiveDataJack->OnDataJackChanged.AddUniqueDynamic(this, &ThisClass::HandleDataJackCarrierChanged);
	}
	
	RefreshJackCarrierDisplay();
}

void UTeammateIndicatorLayer::HandleDataJackCarrierChanged()
{
	RefreshJackCarrierDisplay();
}

void UTeammateIndicatorLayer::RefreshJackCarrierDisplay()
{
	const ABaseTeamPlayerState* Carrier = BoundDataJack.IsValid() ? BoundDataJack->GetCarrierPlayerState() : nullptr;
	
	for (UTeammateIndicatorEntry* Entry : Entries)
	{
		if (Entry == nullptr)
		{
			continue;
		}
		
		const AShooterPlayerState* BoundPS = Entry->GetBoundPlayerState();
		Entry->SetJackCarrierVisible(BoundPS != nullptr && BoundPS == Carrier);
	}
}

void UTeammateIndicatorLayer::RebuildRoster()
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (GS == nullptr)
	{
		return;
	}
	
	const EGameTeam LocalTeam = GetLocalTeam();
	if (LocalTeam == EGameTeam::None)
	{
		// 팀이 정해지지않았으므로 다음 RoundStarting PHase때 다시 시도
		return;
	}
	
	const AShooterPlayerState* LocalPlayerState = GetOwningPlayerState<AShooterPlayerState>();
	
	int32 EntryIndex = 0;
	
	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		AShooterPlayerState* PS = Cast<AShooterPlayerState>(PlayerState);
		if (IsValid(PS) == false)
		{
			continue;
		}
		
		// 본인 제외
		if (PS == LocalPlayerState)
		{
			continue;
		}
		
		const EGameTeam Team = PS->GetTeam();
		
		// 미배정은 아군 X
		if (Team == EGameTeam::None || Team != LocalTeam)
		{
			continue;
		}
		
		if (UTeammateIndicatorEntry* Entry = GetOrCreateEntry(EntryIndex))
		{
			Entry->SetPlayerState(PS);
			++EntryIndex;
		}
	}
	
	// 남는 Entry 비움. 인원이 줄었을때 떠난 사람 Entry가 남아있는걸 막음
	for (int32 Index = EntryIndex; Index < Entries.Num(); ++Index)
	{
		if (Entries[Index])
		{
			Entries[Index]->SetPlayerState(nullptr);
		}
	}
	
	RefreshJackCarrierDisplay();
}

UTeammateIndicatorEntry* UTeammateIndicatorLayer::GetOrCreateEntry(int32 Index)
{
	if (Entries.IsValidIndex(Index))
	{
		return Entries[Index];
	}
	
	if (!ensure(EntryWidgetClass) || Box_Indicators == nullptr)
	{
		return nullptr;
	}
	
	UTeammateIndicatorEntry* NewEntry = CreateWidget<UTeammateIndicatorEntry>(this, EntryWidgetClass);
	if (NewEntry == nullptr)
	{
		return nullptr;
	}
	
	UCanvasPanelSlot* NewSlot = Box_Indicators->AddChildToCanvas(NewEntry);
	if (NewSlot == nullptr)
	{
		return nullptr;
	}
	
	// 1. 앵커 기준은 좌상단 한 점
	NewSlot->SetAnchors(FAnchors(0.f, 0.f));
	// 2. X 0.5 = 머리 기준 가로 중앙, Y 1.0 = 위젯 아래 변이 머리에 닿아 위로 뜬다.
	NewSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	// 3. 닉네임 길이에 따른 폭 
	NewSlot->SetAutoSize(true);
	
	NewEntry->InitializeIndicator(Box_Indicators, NewSlot);
	
	Entries.Add(NewEntry);
	
	return NewEntry;
}

ABaseRoundGameState* UTeammateIndicatorLayer::GetRoundGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<ABaseRoundGameState>() : nullptr;
}

EGameTeam UTeammateIndicatorLayer::GetLocalTeam() const
{
	const ABaseTeamPlayerState* PS = GetOwningPlayerState<ABaseTeamPlayerState>();
	return PS ? PS->GetTeam() : EGameTeam::None;
}
