// ShooterAugmentSelectBanner.cpp
#include "UI/Common/ShooterAugmentSelectBanner.h"

#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentInventoryComponent.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Player/ShooterPlayerState.h"

void UShooterAugmentSelectBanner::NativeConstruct()
{
	Super::NativeConstruct();
	
	ResetTransientState();
	TryBindInventory();
}

void UShooterAugmentSelectBanner::NativeDestruct()
{
	StopCountdown();
	
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InventoryBindRetryTimerHandle);
		World->GetTimerManager().ClearTimer(BannerRefreshTimerHandle);
	}

	if (UAugmentInventoryComponent* Inventory = BoundInv.Get())
	{
		Inventory->OnOffersChanged.RemoveDynamic(
			this, &ThisClass::HandleOffersChanged);
	}
	BoundInv.Reset();
	
	if (bRegistryReadyBound)
	{
		if (UAugmentRegistrySubsystem* Registry = GetRegistry())
		{
			Registry->OnRegistryReady.RemoveDynamic(
				this, &ThisClass::HandleRegistryReady);
		}
		bRegistryReadyBound = false;
	}
	
	ResetTransientState();
	Super::NativeDestruct();
}

void UShooterAugmentSelectBanner::SetPresentationActive(bool bActive)
{
	if (bPresentationActive == bActive)
	{
		return;
	}
	bPresentationActive = bActive;
	
	if (!bActive)
	{
		StopCountdown();
		return;
	}
	
	RefreshBannerKind();
}

void UShooterAugmentSelectBanner::ScheduleBannerRefresh()
{
	UWorld* World = GetWorld();
	if (bBannerRefreshPending || !IsValid(World))
	{
		return;
	}
	
	bBannerRefreshPending = true;
	BannerRefreshTimerHandle = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(
			this, 
			&ThisClass::HandleScheduledBannerRefresh));
}

void UShooterAugmentSelectBanner::TryBindInventory()
{
	if (BoundInv.IsValid())
	{
		return;
	}
	
	UWorld* World = GetWorld();
	UAugmentInventoryComponent* Inv = GetInventory();
	
	if (!IsValid(Inv))
	{
		if (IsValid(World) && !World->GetTimerManager().IsTimerActive(InventoryBindRetryTimerHandle))
		{
			World->GetTimerManager().SetTimer(
				InventoryBindRetryTimerHandle,
				this,
				&ThisClass::TryBindInventory,
				InventoryBindRetryInterval,
				true);
		}
		return;
	}
	
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(InventoryBindRetryTimerHandle);
	}
	
	BoundInv = Inv;
	Inv->OnOffersChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleOffersChanged);
	
	RefreshBannerKind();
}

void UShooterAugmentSelectBanner::ResetTransientState()
{
	BannerKind = EAugmentBannerKind::None;
	SlotDisplayName = FText::GetEmpty();
	DisplaySeconds = 0;
	bHasDeadline = false;
	bPresentationActive = false;
	bRegistryReadyBound = false;
	bBannerRefreshPending = false;
}

void UShooterAugmentSelectBanner::RefreshBannerKind()
{
	if (!bPresentationActive)
	{
		return;
	}
	
	const UAugmentInventoryComponent* Inventory = BoundInv.Get();
	UAugmentRegistrySubsystem* Registry = GetRegistry();
	
	EAugmentBannerKind NewKind = EAugmentBannerKind::None;
	FText NewSlotName;
	
	if (IsValid(Registry) && !Registry->IsReady())
	{
		if (!bRegistryReadyBound)
		{
			Registry->OnRegistryReady.AddUniqueDynamic(
				this, 
				&ThisClass::HandleRegistryReady);
			
			bRegistryReadyBound = true;
		}
	}
	else if (IsValid(Inventory) && IsValid(Registry) && !Inventory->GetOffers().IsEmpty())
	{
		const TArray<FAugmentInventoryEntry>& Offers = Inventory->GetOffers();
		
		int32 ActiveCount = 0;
		FGameplayTag SharedSlotTag;
		bool bAllResolved = true;
		bool bSlotUsable = true;
		
		for (const FAugmentInventoryEntry& Offer : Offers)
		{
			const UAugmentDefinition* Def = Registry->FindDefinition(Offer.DefinitionId);
			if (!IsValid(Def))
			{
				bAllResolved = false;
				break;
			}
			
			if (Def->AugmentCategory != EAugmentCategory::Active)
			{
				continue;
			}
			
			++ActiveCount;
			
			const FGameplayTag SlotTag = Def->GetActiveInputSlotTag();
			if (!SlotTag.IsValid())
			{
				bSlotUsable = false;
			}
			else if (!SharedSlotTag.IsValid())
			{
				SharedSlotTag = SlotTag;
			}
			else if (SharedSlotTag != SlotTag)
			{
				bSlotUsable = false;
			}
		}
		
		const FText* SlotName = bSlotUsable && SharedSlotTag.IsValid()
			? SlotDisplayNames.Find(SharedSlotTag)
			: nullptr;
	
		if (!bAllResolved)
		{
			// 하나라도 못 읽었으면 판정 하지 않음
			NewKind = EAugmentBannerKind::None;
		}
		else if (ActiveCount == 0)
		{
			NewKind = EAugmentBannerKind::Passive;
		}
		else if (ActiveCount == Offers.Num() && SlotName != nullptr)
		{
			NewKind = EAugmentBannerKind::Active;
			NewSlotName = *SlotName;
		}
		else
		{
			// 섞였거나 슬롯이 갈리거나 WBP에 매핑이 없음
			NewKind = EAugmentBannerKind::Mixed;
		}
	}
	
	BannerKind = NewKind;
	SlotDisplayName = NewSlotName;
	BP_OnBannerKindChanged(BannerKind, SlotDisplayName);
	
	if (BannerKind == EAugmentBannerKind::None)
	{
		StopCountdown();
		return;
	}
	
	StartCountdown();
}

void UShooterAugmentSelectBanner::RefreshCountdown()
{
	const UAugmentInventoryComponent* Inv = BoundInv.Get();
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	
	const float Deadline = IsValid(Inv) ? Inv->GetSelectionDeadline() : 0.f;
	
	bHasDeadline = Deadline > 0.f && IsValid(GameState);
	
	if (!bHasDeadline)
	{
		DisplaySeconds = 0;
		BP_OnDisplaySecondsChanged(DisplaySeconds, bHasDeadline);
		StopCountdown();
		return;
	}
	
	const float Remaining = Deadline - GameState->GetServerWorldTimeSeconds();
	
	// 0초를 표시하지않고 시간이 다 되면 타이머를 멈춘다.
	if (Remaining <= 0.f)
	{
		bHasDeadline = false;
		DisplaySeconds = 0;
		
		BP_OnCountdownProgressChanged(1.f);
		BP_OnDisplaySecondsChanged(DisplaySeconds, bHasDeadline);
		
		StopCountdown();
		return;
	}
	
	// 머터리얼 이미지 처리
	const float Progress = 1.f - FMath::Clamp(Remaining / TotalDuration, 0.f, 1.f);
	BP_OnCountdownProgressChanged(Progress);
	
	const int32 NewSeconds = FMath::CeilToInt(Remaining);
	if (NewSeconds == DisplaySeconds)
	{
		return;
	}
	
	DisplaySeconds = NewSeconds;
	BP_OnDisplaySecondsChanged(DisplaySeconds, bHasDeadline);
}

void UShooterAugmentSelectBanner::StartCountdown()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	
	RefreshCountdown();
	
	if (!bHasDeadline)
	{
		return;
	}
	
	World->GetTimerManager().SetTimer(
		CountdownTimerHandle,
		this,
		&ThisClass::RefreshCountdown,
		CountdownInterval,
		true);
}

void UShooterAugmentSelectBanner::StopCountdown()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CountdownTimerHandle);
	}
}

void UShooterAugmentSelectBanner::HandleOffersChanged()
{
	if (!bPresentationActive)
	{
		return;
	}

	
	ScheduleBannerRefresh();
}

void UShooterAugmentSelectBanner::HandleRegistryReady()
{
	if (UAugmentRegistrySubsystem* Registry = GetRegistry())
	{
		Registry->OnRegistryReady.RemoveDynamic(
			this, &ThisClass::HandleRegistryReady);
	}
	bRegistryReadyBound = false;
	
	RefreshBannerKind();
}

void UShooterAugmentSelectBanner::HandleScheduledBannerRefresh()
{
	bBannerRefreshPending = false;
	RefreshBannerKind();
}

UAugmentInventoryComponent* UShooterAugmentSelectBanner::GetInventory() const
{
	const APlayerController* PC = GetOwningPlayer();
	AShooterPlayerState* PS = IsValid(PC)
		? PC->GetPlayerState<AShooterPlayerState>()
		: nullptr;
	
	return IsValid(PS) ? PS->GetAugmentInventory() : nullptr;
}

UAugmentRegistrySubsystem* UShooterAugmentSelectBanner::GetRegistry() const
{
	return GetGameInstance()
		? GetGameInstance()->GetSubsystem<UAugmentRegistrySubsystem>()
		: nullptr;
}

EAugmentBannerKind UShooterAugmentSelectBanner::GetBannerKind() const
{
	return BannerKind;
}

FText UShooterAugmentSelectBanner::GetSlotDisplayName() const
{
	return SlotDisplayName;
}

int32 UShooterAugmentSelectBanner::GetDisplaySeconds() const
{
	return DisplaySeconds;
}

bool UShooterAugmentSelectBanner::HasSelectionDeadline() const
{
	return bHasDeadline;
}
