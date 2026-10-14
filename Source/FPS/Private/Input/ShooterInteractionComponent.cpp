#include "Input/ShooterInteractionComponent.h"

#include "CommonActivatableWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/RogueInteractable.h"
#include "Player/ShooterPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Tags/UITags.h"
#include "UI/Layout/ShooterPrimaryGameLayout.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

UShooterInteractionComponent::UShooterInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    SetIsReplicatedByDefault(true);

    InteractionPromptBlockingLayers = {
        UITags::UI_Layer_Menu,
        UITags::UI_Layer_Modal
    };
}

void UShooterInteractionComponent::BeginPlay()
{
    Super::BeginPlay();

    PrimaryComponentTick.TickInterval = FMath::Max(0.01f, InteractionPromptRefreshIntervalSeconds);
    UpdatePromptTickEnabled();
}

void UShooterInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ActiveInteractionPrompt)
    {
        ActiveInteractionPrompt->RemoveFromParent();
        ActiveInteractionPrompt = nullptr;
    }
    ProximityInteractables.Reset();
    Super::EndPlay(EndPlayReason);
}

void UShooterInteractionComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    RefreshInteractionPrompt();
}

AActor* UShooterInteractionComponent::FindInteractableInView() const
{
    const AShooterPlayerController* Player = Cast<AShooterPlayerController>(GetOwner());
    if (!Player || !Player->GetPawn() || !Player->CanProcessActionInput())
    {
        return nullptr;
    }

    FVector ViewOrigin;
    FRotator ViewRotation;
    Player->GetPlayerViewPoint(ViewOrigin, ViewRotation);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShooterInteraction), false, Player->GetPawn());
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, ViewOrigin,
        ViewOrigin + ViewRotation.Vector() * InteractionDistanceCentimeters,
        InteractionTraceChannel, QueryParams))
    {
        return nullptr;
    }

    AActor* Candidate = Hit.GetActor();
    const IRogueInteractable* Interactable = Cast<IRogueInteractable>(Candidate);
    return Interactable && Interactable->CanInteract(*Player) ? Candidate : nullptr;
}

AActor* UShooterInteractionComponent::FindProximityInteractable() const
{
    const AShooterPlayerController* Player = Cast<AShooterPlayerController>(GetOwner());
    const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
    if (!Pawn)
    {
        return nullptr;
    }

    AActor* BestCandidate = nullptr;
    double BestDistanceSquared = TNumericLimits<double>::Max();
    for (const TWeakObjectPtr<AActor>& CandidatePtr : ProximityInteractables)
    {
        AActor* Candidate = CandidatePtr.Get();
        if (!Candidate || !CanReachInteractable(Candidate))
        {
            continue;
        }

        const double DistanceSquared = FVector::DistSquared(Pawn->GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSquared < BestDistanceSquared)
        {
            BestDistanceSquared = DistanceSquared;
            BestCandidate = Candidate;
        }
    }
    return BestCandidate;
}

AActor* UShooterInteractionComponent::FindBestInteractable() const
{
    if (AActor* Viewed = FindInteractableInView())
    {
        return Viewed;
    }
    return FindProximityInteractable();
}

void UShooterInteractionComponent::UpdatePromptTickEnabled()
{
    const APlayerController* Player = Cast<APlayerController>(GetOwner());
    const bool bShouldTick = Player && Player->IsLocalController() && InteractionPromptWidgetClass != nullptr;
    SetComponentTickEnabled(bShouldTick);
    if (!bShouldTick)
    {
        HideInteractionPrompt();
    }
}

bool UShooterInteractionComponent::IsInteractionPromptBlockedByUI() const
{
    const APlayerController* Player = Cast<APlayerController>(GetOwner());
    UGameInstance* GameInstance = Player ? Player->GetGameInstance() : nullptr;
    UShooterUIManagerSubsystem* UIManager = GameInstance
        ? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
        : nullptr;
    if (!IsValid(UIManager) || !UIManager->IsUIInitialized())
    {
        return false;
    }

    UShooterPrimaryGameLayout* RootLayout = UIManager->GetRootLayout();
    if (!IsValid(RootLayout))
    {
        return false;
    }

    for (const FGameplayTag& LayerTag : InteractionPromptBlockingLayers)
    {
        if (!LayerTag.IsValid())
        {
            continue;
        }

        UCommonActivatableWidgetContainerBase* Layer = RootLayout->GetLayerContainer(LayerTag);
        if (IsValid(Layer) && IsValid(Layer->GetActiveWidget()))
        {
            return true;
        }
    }

    return false;
}

bool UShooterInteractionComponent::TryInteract()
{
    AActor* Candidate = FindBestInteractable();
    if (!Candidate)
    {
        return false;
    }

    HideInteractionPrompt();
    ServerInteract(Candidate);
    return true;
}

void UShooterInteractionComponent::RegisterProximityInteractable(AActor* Candidate)
{
    if (!IsValid(Candidate))
    {
        return;
    }

    ProximityInteractables.RemoveAll([](const TWeakObjectPtr<AActor>& Entry)
    {
        return !Entry.IsValid();
    });
    ProximityInteractables.AddUnique(TWeakObjectPtr<AActor>(Candidate));
    RefreshInteractionPrompt();
}

void UShooterInteractionComponent::UnregisterProximityInteractable(AActor* Candidate)
{
    ProximityInteractables.RemoveAll([Candidate](const TWeakObjectPtr<AActor>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == Candidate;
    });

    RefreshInteractionPrompt();
}

void UShooterInteractionComponent::SetInteractionPromptWidgetClass(TSubclassOf<UUserWidget> InWidgetClass)
{
    if (InteractionPromptWidgetClass == InWidgetClass)
    {
        return;
    }

    InteractionPromptWidgetClass = InWidgetClass;
    if (ActiveInteractionPrompt)
    {
        ActiveInteractionPrompt->RemoveFromParent();
        ActiveInteractionPrompt = nullptr;
    }
    UpdatePromptTickEnabled();
}

void UShooterInteractionComponent::SetInteractionPromptZOrder(const int32 InZOrder)
{
    const int32 NewZOrder = FMath::Max(0, InZOrder);
    if (InteractionPromptZOrder == NewZOrder)
    {
        return;
    }

    InteractionPromptZOrder = NewZOrder;
    if (ActiveInteractionPrompt)
    {
        ActiveInteractionPrompt->RemoveFromParent();
        ActiveInteractionPrompt = nullptr;
    }
}

bool UShooterInteractionComponent::CanReachInteractable(const AActor* Candidate) const
{
    const AShooterPlayerController* Player = Cast<AShooterPlayerController>(GetOwner());
    return Player && Player->CanProcessActionInput() && CanReachAcceptedInteractable(Candidate);
}

bool UShooterInteractionComponent::CanReachAcceptedInteractable(const AActor* Candidate) const
{
    const AShooterPlayerController* Player = Cast<AShooterPlayerController>(GetOwner());
    const IRogueInteractable* Interactable = Cast<IRogueInteractable>(Candidate);
    if (!Player || !Player->GetPawn() || !IsValid(Candidate) || !Interactable
        || Candidate->GetWorld() != GetWorld() || !Interactable->CanInteract(*Player))
    {
        return false;
    }

    FVector ViewOrigin;
    FRotator ViewRotation;
    Player->GetPlayerViewPoint(ViewOrigin, ViewRotation);
    const FVector Destination = Candidate->GetActorLocation();
    if (FVector::DistSquared(ViewOrigin, Destination) > FMath::Square(InteractionDistanceCentimeters))
    {
        return false;
    }

    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShooterInteractionConfirm), false, Player->GetPawn());
    const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
        Hit, ViewOrigin, Destination, InteractionTraceChannel, QueryParams);
    return !bBlocked || Hit.GetActor() == Candidate;
}

void UShooterInteractionComponent::RefreshInteractionPrompt()
{
    const APlayerController* Player = Cast<APlayerController>(GetOwner());
    if (!Player || !Player->IsLocalController() || !InteractionPromptWidgetClass)
    {
        HideInteractionPrompt();
        return;
    }

    UGameInstance* GameInstance = Player->GetGameInstance();
    UShooterUIManagerSubsystem* UIManager = GameInstance
        ? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
        : nullptr;
    if (!IsValid(UIManager) || !UIManager->IsUIInitialized())
    {
        HideInteractionPrompt();
        return;
    }

    if (IsInteractionPromptBlockedByUI() || !FindBestInteractable())
    {
        HideInteractionPrompt();
        return;
    }

    ShowInteractionPrompt();
}

void UShooterInteractionComponent::ShowInteractionPrompt()
{
    APlayerController* Player = Cast<APlayerController>(GetOwner());
    if (!Player || !Player->IsLocalController())
    {
        return;
    }

    if (!ActiveInteractionPrompt)
    {
        if (!InteractionPromptWidgetClass)
        {
            return;
        }

        ActiveInteractionPrompt = CreateWidget<UUserWidget>(Player, InteractionPromptWidgetClass);
        if (!ActiveInteractionPrompt)
        {
            return;
        }
        ActiveInteractionPrompt->AddToViewport(InteractionPromptZOrder);
    }

    ActiveInteractionPrompt->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UShooterInteractionComponent::HideInteractionPrompt()
{
    if (ActiveInteractionPrompt)
    {
        ActiveInteractionPrompt->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UShooterInteractionComponent::ServerInteract_Implementation(AActor* Candidate)
{
    if (CanReachInteractable(Candidate))
    {
        IRogueInteractable* Interactable = Cast<IRogueInteractable>(Candidate);
        Interactable->Interact(*CastChecked<APlayerController>(GetOwner()));
    }
}
