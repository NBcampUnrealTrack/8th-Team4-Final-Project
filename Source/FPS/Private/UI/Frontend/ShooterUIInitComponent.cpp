// ShooterUIInitComponent.cpp
#include "UI/Frontend/ShooterUIInitComponent.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/UITags.h"
#include "UI/InGame/ShooterAugmentSelectScreen.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterUIInitComp, Log, All);

void UShooterUIInitComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!ensure(PC))
	{
		return;
	}

	if (!PC->IsLocalController())
	{
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (IsValid(UIManager) == false)
	{
		return;
	}
	
	UIManager->InitializeUI(PC, RootLayoutClass);

	// BP에서 지정
	if (!ensure(InitialWidgetClass))
	{
		return;
	}

	TryStartInGameUI();
}

void UShooterUIInitComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlayerStateWaitTimerHandle);
	}
	
	Super::EndPlay(EndPlayReason);
}

void UShooterUIInitComponent::TryStartInGameUI()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	APlayerState* PS = GetOwner<APlayerController>()->GetPlayerState<AShooterPlayerState>();
	if (!PS)
	{
		if (!World->GetTimerManager().IsTimerActive(PlayerStateWaitTimerHandle))
		{
			GetWorld()->GetTimerManager().SetTimer(
			PlayerStateWaitTimerHandle,
			this,
			&ThisClass::TryStartInGameUI,
			0.1f,
			true
			);
		}
		
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(PlayerStateWaitTimerHandle);
	
	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (IsValid(UIManager) == false)
	{
		return;
	}
	
	UIManager->PushWidgetToLayer(InitialWidgetLayer, InitialWidgetClass);
}

void UShooterUIInitComponent::ShowAugmentSelect(const TArray<FAugmentInventoryEntry>& InOffers)
{
	// 이미 증강이 떠있으면 return
	if (AugmentSelectInstance && AugmentSelectInstance->IsInViewport())
	{
		AugmentSelectInstance->SetOffers(InOffers);
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (IsValid(UIManager) == false)
	{
		return;
	}

	if (IsValid(AugmentSelectClass) == false)
	{
		UE_LOG(LogShooterUIInitComp, Error, TEXT("UIInitComponent | AugmentSelectClass가 지정되지 않았습니다"));
		return;
	}
	
	AugmentSelectInstance = Cast<UShooterAugmentSelectScreen>(UIManager->PushWidgetToLayer(UITags::UI_Layer_Menu, AugmentSelectClass));
	if (IsValid(AugmentSelectInstance) == false)
	{
		return;
	}
	
	AugmentSelectInstance->OnAugmentSelected.AddDynamic(this, &ThisClass::HandleAugmentSelected);
	AugmentSelectInstance->SetOffers(InOffers);
}

void UShooterUIInitComponent::HideAugmentSelect()
{
	if (AugmentSelectInstance)
	{
		AugmentSelectInstance->DeactivateWidget();
		AugmentSelectInstance = nullptr;
	}
}

void UShooterUIInitComponent::HandleAugmentSelected(int32 OfferIndex)
{
	OnAugmentSelected.Broadcast(OfferIndex);
	// DeactivateWidget은 SelectScreen에서 처리 했으므로 Instance만 비움
	AugmentSelectInstance = nullptr;
}

UShooterUIManagerSubsystem* UShooterUIInitComponent::GetUIManager() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;

	return GI ? GI->GetSubsystem<UShooterUIManagerSubsystem>() : nullptr;
}
