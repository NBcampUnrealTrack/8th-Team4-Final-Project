// ShooterTeamStatus.cpp
#include "UI/InGame/HUD/ShooterTeamStatus.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBoxSlot.h"
#include "GameMode/BaseRoundGameState.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/ShooterPlayerState.h"
#include "UI/InGame/HUD/ShooterPlayerSlot.h"
#include "GameMode/RoundTypes.h"

void UShooterTeamStatus::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	if (ABaseRoundGameState* GS = GetRoundGameState())
	{
		BoundGameState = GS;
		GS->OnRoundStateChanged.AddDynamic(this, &ThisClass::HandleRoundStateChanged);
	}
	
	// 위젯이 라운드 시작 뒤에 만들어졌을 수 있으므로 즉시 1회 시도
	RebuildRoster();
}

void UShooterTeamStatus::NativeDestruct()
{
	if (BoundGameState.IsValid())
	{
		BoundGameState->OnRoundStateChanged.RemoveAll(this);
	}
	
	BoundGameState.Reset();
	
	Super::NativeDestruct();
}

void UShooterTeamStatus::HandleRoundStateChanged()
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

void UShooterTeamStatus::RebuildRoster()
{
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (GS == nullptr)
	{
		return;
	}
	
	const EGameTeam LocalTeam = GetLocalTeam();

	if (LocalTeam == EGameTeam::None)
	{
		
		return;
	}
	
	int32 SlotIndex = 0;
	
	for (APlayerState* PlayerState : GS->PlayerArray)
	{
		AShooterPlayerState* PS = Cast<AShooterPlayerState>(PlayerState);
		if (IsValid(PS) == false)
		{
			continue;
		}
		
		const EGameTeam Team = PS->GetTeam();
		
		// 미배정은 처리
		if (Team == EGameTeam::None)
		{
			continue;
		}
		
		const bool bIsLocalTeam = (Team == LocalTeam);
		if (bIsLocalTeam != bShowLocalTeam)
		{
			continue;
		}
		
		if (UShooterPlayerSlot* TeamSlot = GetOrCreateSlot(SlotIndex))
		{
			TeamSlot->SetPlayerState(PS);
			++SlotIndex;
		}
	}
	
	// 라운드 교체시 인원이 줄었으면 인원 수 교체
	for (int32 Index = SlotIndex; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index])
		{
			Slots[Index]->SetPlayerState(nullptr);
		}
	}
}

UShooterPlayerSlot* UShooterTeamStatus::GetOrCreateSlot(int32 Index)
{
	if (Slots.IsValidIndex(Index))
	{
		return Slots[Index];
	}
	
	if (!ensure(SlotWidgetClass) || Box_Slots == nullptr)
	{
		return nullptr;
	}
	
	UShooterPlayerSlot* NewSlot = CreateWidget<UShooterPlayerSlot>(this, SlotWidgetClass);
	if (NewSlot == nullptr)
	{
		return nullptr;
	}
	
	NewSlot->SetSlotStyle(bShowLocalTeam, bShowLocalTeam ? AllyColor : EnemyColor);
	
	Box_Slots->AddChildToHorizontalBox(NewSlot);
	
	if (bShowLocalTeam)
	{
		Box_Slots->ShiftChild(0, NewSlot);
	}
	
	Slots.Add(NewSlot);
	
	return NewSlot;
}

ABaseRoundGameState* UShooterTeamStatus::GetRoundGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<ABaseRoundGameState>() : nullptr;
}

EGameTeam UShooterTeamStatus::GetLocalTeam() const
{
	const ABaseTeamPlayerState* PS = GetOwningPlayerState<ABaseTeamPlayerState>();
	return PS ? PS->GetTeam() : EGameTeam::None;
}

void UShooterTeamStatus::DebugFillSlots(int32 Count)
{
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (UShooterPlayerSlot* PlayerSlot = GetOrCreateSlot(Index))
		{
			PlayerSlot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}
}
