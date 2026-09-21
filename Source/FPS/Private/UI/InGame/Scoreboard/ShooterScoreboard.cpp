#include "UI/InGame/Scoreboard/ShooterScoreboard.h"

#include "GameFramework/GameStateBase.h"
#include "Player/BaseTeamPlayerState.h"
#include "TimerManager.h"
#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentInventoryComponent.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Interfaces/PlayerInterface.h"
#include "Player/ShooterPlayerState.h"
#include "UI/ShooterUIBundles.h"
#include "UI/Common/ShooterAugmentTierStyleSet.h"
#include "UI/InGame/Scoreboard/ShooterScoreboardAugmentTooltip.h"
#include "UI/InGame/Weapons/ShooterWeaponDisplaySet.h"
#include "Weapon/WeaponBase.h"

void UShooterScoreboard::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!ensure(TierStyles))
	{
		return;
	}
}

void UShooterScoreboard::NativeOnActivated()
{
	Super::NativeOnActivated();

	RefreshAll();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			this,
			&UShooterScoreboard::RefreshAll,
			RefreshInterval,
			true);
	}
}

void UShooterScoreboard::NativeOnDeactivated()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	// MouseLeave가 안 오는 경우까지 포함해서 강제로 닫는다.
	ClearAugmentTooltip();

	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}
	LoadedIconIds.Empty();

	Super::NativeOnDeactivated();
}

void UShooterScoreboard::RefreshAll()
{
	AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (GameState == nullptr || RowWidgetClass == nullptr)
	{
		return;
	}

	// 내 팀을 모르면 이번 틱은 건너뜀
	const ABaseTeamPlayerState* LocalPS = Cast<ABaseTeamPlayerState>(GetOwningPlayerState());
	if (!IsValid(LocalPS) || LocalPS->GetTeam() == EGameTeam::None)
	{
		SetTeamBoxesVisible(false);

		ClearAugmentTooltip();
		return;
	}
	SetTeamBoxesVisible(true);

	const EGameTeam LocalTeam = LocalPS->GetTeam();

	// 1. 살아있는 PlayerState를 팀별로 모음
	TArray<APlayerState*> MyTeam;
	TArray<APlayerState*> EnemyTeam;

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		ABaseTeamPlayerState* TeamPS = Cast<ABaseTeamPlayerState>(PlayerState);
		if (!IsValid(TeamPS))
		{
			continue;
		}

		if (TeamPS->GetTeam() == EGameTeam::None)
		{
			continue;
		}

		(TeamPS->GetTeam() == LocalTeam ? MyTeam : EnemyTeam).Add(PlayerState);
	}

	// 2. 이름 순으로 정렬 동명이인은 PlayerArray 내부 순서에 맡겨짐
	auto SortByName = [](TArray<APlayerState*>& Players)
	{
		Players.Sort([](const APlayerState& A, const APlayerState& B)
		{
			const int32 NameCompare = A.GetPlayerName().Compare(B.GetPlayerName());
			return NameCompare != 0 ? (NameCompare < 0) : (A.GetPlayerId() < B.GetPlayerId());
		});
	};
	SortByName(MyTeam);
	SortByName(EnemyTeam);

	// 3. 사라진 Row 제거
	TSet<TObjectKey<APlayerState>> Present;
	Present.Reserve(MyTeam.Num() + EnemyTeam.Num());
	for (APlayerState* PS : MyTeam)
	{
		Present.Add(TObjectKey<APlayerState>(PS));
	}
	for (APlayerState* PS : EnemyTeam)
	{
		Present.Add(TObjectKey<APlayerState>(PS));
	}

	for (auto It = RowCache.CreateIterator(); It; ++It)
	{
		if (Present.Contains(It.Key()))
		{
			continue;;
		}

		if (UShooterScoreboardEntry* Row = It.Value().Get())
		{
			// Hover중인 칩을 가진 Row가 사라지면 MouseLeave가 오지않음
			if (HoveredChip.IsValid() && Row->OwnsAugmentChip(HoveredChip.Get()))
			{
				ClearAugmentTooltip();
			}

			Row->RemoveFromParent();
		}
		It.RemoveCurrent();
	}

	// 4. 팀별로 생성/갱신/쟂어렬
	ApplyRows(MyTeam, Box_MyTeamRows, EShooterResultRowSide::LocalTeam);
	ApplyRows(EnemyTeam, Box_EnemyTeamRows, EShooterResultRowSide::EnemyTeam);
}

void UShooterScoreboard::ApplyRows(const TArray<APlayerState*>& Players, UPanelWidget* Box, EShooterResultRowSide Side)
{
	if (Box == nullptr)
	{
		return;
	}

	for (int32 i = 0; i < Players.Num(); ++i)
	{
		APlayerState* PlayerState = Players[i];
		const TObjectKey<APlayerState> Key(PlayerState);

		TWeakObjectPtr<UShooterScoreboardEntry>* Found = RowCache.Find(Key);
		UShooterScoreboardEntry* Row = Found ? Found->Get() : nullptr;

		if (!IsValid(Row))
		{
			Row = CreateWidget<UShooterScoreboardEntry>(this, RowWidgetClass);
			if (Row == nullptr)
			{
				continue;
			}

			Box->AddChild(Row);
			RowCache.Add(Key, Row);

			Row->OnAugmentHovered.AddWeakLambda(this,
			                                    [this](const FShooterAugmentDisplay& Data, UShooterAugmentChip* Chip)
			                                    {
				                                    ShowAugmentTooltip(Data, Chip);
			                                    });
			Row->OnAugmentUnHovered.AddWeakLambda(this,
			                                      [this](UShooterAugmentChip* Chip)
			                                      {
				                                      HideAugmentTooltip(Chip);
			                                      });
		}
		else if (Row->GetParent() != Box)
		{
			// 팀이 바뀐 경우(진영 교대 등). 위젯은 부수지 않고 옮기기만 한다
			Row->RemoveFromParent();
			Box->AddChild(Row);
		}

		// 팀 색은 SetData 안에서 BP_ApplyTeamStyle 로 적용된다(결과 화면과 같은 구조)
		Row->SetData(BuildRowData(PlayerState), bStatsAvailable, Side);
		Box->ShiftChild(i, Row);
	}
}

void UShooterScoreboard::SetTeamBoxesVisible(bool bVisible)
{
	const ESlateVisibility BoxVisibility =
		bVisible
			? ESlateVisibility::Visible
			: ESlateVisibility::Hidden;

	Box_MyTeamRows->SetVisibility(BoxVisibility);
	Box_EnemyTeamRows->SetVisibility(BoxVisibility);
}

void UShooterScoreboard::ShowAugmentTooltip(const FShooterAugmentDisplay& Data, UShooterAugmentChip* Chip)
{
	if (!ensure(Canvas_TooltipLayer) || !ensure(TooltipClass))
	{
		return;
	}

	if (Chip == nullptr)
	{
		return;
	}

	if (!IsValid(TooltipInstance))
	{
		TooltipInstance = CreateWidget<UShooterScoreboardAugmentTooltip>(this, TooltipClass);
		if (TooltipInstance == nullptr)
		{
			return;
		}

		UCanvasPanelSlot* NewSlot = Canvas_TooltipLayer->AddChildToCanvas(TooltipInstance);
		if (NewSlot)
		{
			NewSlot->SetAutoSize(true);
			NewSlot->SetAlignment(FVector2D::ZeroVector);
		}
	}

	HoveredChip = Chip;
	TooltipInstance->SetData(Data);
	TooltipInstance->SetVisibility(ESlateVisibility::HitTestInvisible);

	// 방금 SetData한 텍스트가 반영된 크기를 얻기위해 레이아웃을 먼저 돌림
	TooltipInstance->ForceLayoutPrepass();
	const FVector2D TooltipSize = TooltipInstance->GetDesiredSize();

	const FGeometry& CanvasGeo = Canvas_TooltipLayer->GetCachedGeometry();
	const FGeometry& ChipGeo = Chip->GetCachedGeometry();

	const FVector2D ChipTop = CanvasGeo.AbsoluteToLocal(
		ChipGeo.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.f)));
	const FVector2D ChipBottom = CanvasGeo.AbsoluteToLocal(
		ChipGeo.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 1.f)));
	const FVector2D CanvasSize = CanvasGeo.GetLocalSize();

	// 세로로 보이며 위 우선, 안들어가면 아래, 둘 다 안되면 넓은 쪽
	const float SpaceAbove = ChipTop.Y - TooltipViewportPadding;
	const float SpaceBelow = CanvasSize.Y - ChipBottom.Y - TooltipViewportPadding;

	const bool bFitsAbove = TooltipSize.Y + TooltipGap <= SpaceAbove;
	const bool bFitsBelow = TooltipSize.Y + TooltipGap <= SpaceBelow;

	const bool bPlaceAbove = bFitsAbove || (!bFitsBelow && SpaceAbove >= SpaceBelow);

	FVector2D Position;
	Position.X = ChipTop.X - TooltipSize.X * 0.5f;
	Position.Y = bPlaceAbove
		             ? ChipTop.Y - TooltipSize.Y - TooltipGap
		             : ChipBottom.Y + TooltipGap;

	// 툴팁이 캔버스보다 크면 Max가 Min보다 작아지므로 clamp로 정리
	const float MaxX = FMath::Max(
		TooltipViewportPadding,
		CanvasSize.X - TooltipSize.X - TooltipViewportPadding);
	Position.X = FMath::Clamp(Position.X, TooltipViewportPadding, MaxX);

	const float MaxY = FMath::Max(
		TooltipViewportPadding,
		CanvasSize.Y - TooltipSize.Y - TooltipViewportPadding);
	Position.Y = FMath::Clamp(Position.Y, TooltipViewportPadding, MaxY);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(TooltipInstance->Slot))
	{
		CanvasSlot->SetPosition(Position);
	}
}

void UShooterScoreboard::HideAugmentTooltip(UShooterAugmentChip* Chip)
{
	if (HoveredChip.Get() != Chip)
	{
		return;
	}

	ClearAugmentTooltip();
}

void UShooterScoreboard::ClearAugmentTooltip()
{
	HoveredChip.Reset();

	if (IsValid(TooltipInstance))
	{
		TooltipInstance->SetVisibility(ESlateVisibility::Collapsed);
	}
}

FShooterScoreboardRowData UShooterScoreboard::BuildRowData(APlayerState* PlayerState)
{
	FShooterScoreboardRowData Data;

	if (!ensure(PlayerState))
	{
		return Data;
	}

	Data.PlayerName = FText::FromString(PlayerState->GetPlayerName());
	Data.PingMS = FMath::RoundToInt(PlayerState->GetPingInMilliseconds());

	if (const ABaseTeamPlayerState* TeamPS = Cast<ABaseTeamPlayerState>(PlayerState))
	{
		Data.bIsAlive = TeamPS->IsAlive();
	}

	// K/D 
	if (const AShooterPlayerState* ShooterPS = Cast<AShooterPlayerState>(PlayerState))
	{
		Data.Kills = ShooterPS->GetKills();
		Data.Deaths = ShooterPS->GetDeaths();
	}

	// Assists 는 소스가 없으므로 기본값 유지
	Data.WeaponName = ResolveWeaponText(PlayerState);
	Data.Augments = ResolveAugments(PlayerState);

	return Data;
}

FText UShooterScoreboard::ResolveWeaponText(APlayerState* PlayerState) const
{
	APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;

	if (Pawn == nullptr || !Pawn->Implements<UPlayerInterface>())
	{
		return FText::GetEmpty();
	}
	
	AWeaponBase* Weapon = IPlayerInterface::Execute_GetCurrentWeapon(Pawn);
	if (Pawn == nullptr || !Pawn->Implements<UPlayerInterface>())
	{
		return FText::GetEmpty();
	}

	if (const FShooterWeaponDisplay* Display = WeaponDisplaySet->FindDisplay(Weapon->GetWeaponTag()))
	{
		return Display->DisplayName;
	}

	return FText::GetEmpty();
}

TArray<FShooterAugmentDisplay> UShooterScoreboard::ResolveAugments(APlayerState* PlayerState)
{
	TArray<FShooterAugmentDisplay> Result;

	const AShooterPlayerState* PS = Cast<AShooterPlayerState>(PlayerState);
	const UAugmentInventoryComponent* Inventory = PS ? PS->GetAugmentInventory() : nullptr;
	UAugmentRegistrySubsystem* Registry = GetGameInstance()
		                                      ? GetGameInstance()->GetSubsystem<UAugmentRegistrySubsystem>()
		                                      : nullptr;

	if (Inventory == nullptr || Registry == nullptr)
	{
		return Result;
	}

	const TArray<FAugmentInventoryEntry>& Entries = Inventory->GetEntries();

	EnsureAugmentIconsLoaded(Entries);

	for (const FAugmentInventoryEntry& Entry : Entries)
	{
		const UAugmentDefinition* Definition = Registry->FindDefinition(Entry.DefinitionId);
		if (!IsValid(Definition))
		{
			continue;
		}

		FShooterAugmentDisplay& Display = Result.AddDefaulted_GetRef();
		Display.DisplayName = Definition->DisplayName;
		Display.Tier = Definition->Tier;
		Display.Icon = Definition->Icon;

		const FShooterAugmentTierStyle* Style =
			TierStyles ? TierStyles->FindStyle(Definition->Tier) : nullptr;

		if (Style != nullptr)
		{
			Display.TierDisplayName = Style->TierDisplayName;
			Display.TierAccentColor = Style->AccentColor;
		}
		else
		{
			// fallback 
			Display.TierDisplayName = FText::GetEmpty();
			Display.TierAccentColor = TierFallbackColor;
		}

		if (const FAugmentLevelData* LevelData = Definition->GetLevelData())
		{
			Display.Description = LevelData->Description;
		}
	}

	return Result;
}

void UShooterScoreboard::EnsureAugmentIconsLoaded(const TArray<FAugmentInventoryEntry>& Entries)
{
	TArray<FPrimaryAssetId> Ids;
	Ids.Reserve(Entries.Num());
	for (const FAugmentInventoryEntry& Entry : Entries)
	{
		Ids.Add(Entry.DefinitionId);
	}

	if (Ids == LoadedIconIds)
	{
		return;
	}
	LoadedIconIds = Ids;

	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}

	if (Ids.IsEmpty())
	{
		return;
	}

	IconLoadHandle = UAssetManager::Get().ChangeBundleStateForPrimaryAssets(
		Ids, {FShooterUIBundles::Icon}, {});
}
