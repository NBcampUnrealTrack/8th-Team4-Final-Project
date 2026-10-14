// ShooterScoreboardEntry.cpp
#include "UI/InGame/Scoreboard/ShooterScoreboardEntry.h"

#include "CommonTextBlock.h"
#include "Components/PanelWidget.h"

void UShooterScoreboardEntry::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	// BindWidget 이라 여기 도달했으면 8개 다 유효
	AugmentChips = {
		Chip_Augment0, Chip_Augment1, Chip_Augment2, Chip_Augment3,
		Chip_Augment4, Chip_Augment5, Chip_Augment6, Chip_Augment7
	};

	for (const TObjectPtr<UShooterAugmentChip>& Chip : AugmentChips)
	{
		if (Chip == nullptr)
		{
			continue;
		}

		// 칩 → Row → 스코어보드. 위로 그대로 흘려보낸다
		Chip->OnChipHovered.AddWeakLambda(this,
			[this](const FShooterAugmentDisplay& Data, UShooterAugmentChip* HoveredChip)
			{
				OnAugmentHovered.Broadcast(Data, HoveredChip);
			});
		Chip->OnChipUnHovered.AddWeakLambda(this,
			[this](UShooterAugmentChip* LeftChip)
			{
				OnAugmentUnHovered.Broadcast(LeftChip);
			});
	}
	
}

void UShooterScoreboardEntry::SetData(
	const FShooterScoreboardRowData& InData,
	bool bStatsAvailable,
	EShooterResultRowSide Side)
{
	// 기본 정보
	Text_PlayerName->SetText(InData.PlayerName);
	Text_Weapon->SetText(InData.WeaponName);
	Text_Ping->SetText(FText::AsNumber(InData.PingMS));

	// 팀 스타일 적용
	if (!CachedSide.IsSet() || CachedSide.GetValue() != Side)
	{
		CachedSide = Side;
		BP_ApplyTeamStyle(Side);
	}

	SetRenderOpacity(InData.bIsAlive ? 1.0f : DeadRowOpacity);

	if (Icon_Dead)
	{
		Icon_Dead->SetVisibility(
			InData.bIsAlive
				? ESlateVisibility::Hidden
				: ESlateVisibility::Visible);
	}

	// KDA
	if (Text_Kills)
	{
		Text_Kills->SetText(FText::AsNumber(InData.Kills));
	}

	if (Text_Deaths)
	{
		Text_Deaths->SetText(FText::AsNumber(InData.Deaths));
	}

	if (Text_Assists)
	{
		// Text_Assists->SetText(FText::AsNumber(InData.Assists));
	}

	if (Panel_Stats)
	{
		Panel_Stats->SetVisibility(
			bStatsAvailable
				? ESlateVisibility::Visible
				: ESlateVisibility::Hidden);
	}

	// 액티브는 슬롯 번호 칸에
	for (int32 i = 0; i < ActiveChipCount; ++i)
	{
		const FShooterAugmentDisplay* ActiveAugment = InData.Augments.FindByPredicate(
			[i](const FShooterAugmentDisplay& Augment)
			{
				return Augment.ActiveSlotIndex == i;
			});

		if (ActiveAugment)
		{
			AugmentChips[i]->SetData(*ActiveAugment);
		}
		else
		{
			AugmentChips[i]->SetEmpty();
		}
	}

	// 패시브는 나머지 칸에 차례로
	int32 ChipIndex = ActiveChipCount;
	for (const FShooterAugmentDisplay& Augment : InData.Augments)
	{
		if (!AugmentChips.IsValidIndex(ChipIndex))
		{
			break;
		}

		if (Augment.ActiveSlotIndex == INDEX_NONE)
		{
			AugmentChips[ChipIndex++]->SetData(Augment);
		}
	}

	for (; ChipIndex < AugmentChips.Num(); ++ChipIndex)
	{
		AugmentChips[ChipIndex]->SetEmpty();
	}
}

bool UShooterScoreboardEntry::OwnsAugmentChip(const UShooterAugmentChip* Chip) const
{
	for (const TObjectPtr<UShooterAugmentChip>& Entry : AugmentChips)
	{
		if (Entry.Get() == Chip)
		{
			return true;
		}
	}

	return false;
}


