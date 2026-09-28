// ScreenTransitionSubsystem.cpp
#include "UI/Transition/ScreenTransitionSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UI/ShooterUISettings.h"
#include "UI/Transition/ScreenTransitionWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogScreenTransition, Log, All);

namespace
{
	constexpr int32 ScreenTransitionZOrder = 11000;
}

bool UScreenTransitionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Dedicated Server에서는 UI가 필요 없으므로 서브시스템을 생성하지 않는다.
	const UGameInstance* GameInstance = Cast<UGameInstance>(Outer);
	return Super::ShouldCreateSubsystem(Outer)
		&& IsValid(GameInstance)
		&& !GameInstance->IsDedicatedServerInstance();
}

void UScreenTransitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 전환을 시작하는 순간에 로드하지 않도록 선로드
	const UShooterUISettings* Settings = GetDefault<UShooterUISettings>();
	if (Settings->ScreenTransitionWidgetClass.IsNull())
	{
		UE_LOG(LogScreenTransition, Error, TEXT("ScreenTransitionWidgetClass 가 지정되지 않았습니다."));
		return;
	}

	ResolvedWidgetClass = Settings->ScreenTransitionWidgetClass.LoadSynchronous();
	if (!ResolvedWidgetClass)
	{
		UE_LOG(LogScreenTransition, Error, TEXT("ScreenTransitionWidgetClass 를 로드하지 못했습니다."));
	}
}

void UScreenTransitionSubsystem::Deinitialize()
{
	TearDownWidget();
	Super::Deinitialize();
}

bool UScreenTransitionSubsystem::BeginTransition(FSimpleDelegate OnCovered, const bool bInstant)
{
	switch (State)
	{
	case EState::Covered:
		OnCovered.ExecuteIfBound();
		return true;

	case EState::Covering:
		ensureMsgf(false, TEXT("덮는 중에 BeginTransition 이 다시 왔다. 한 번에 한 흐름만 덮개를 소유한다"));
		return false;

	case EState::Revealing:
		return false;

	case EState::Idle:
		break;
	}

	UGameViewportClient* ViewportClient = GetGameViewportClient();
	if (!ResolvedWidgetClass || !ViewportClient)
	{
		return false;
	}

	ActiveWidget = CreateWidget<UScreenTransitionWidget>(GetGameInstance(), ResolvedWidgetClass);
	if (!IsValid(ActiveWidget))
	{
		return false;
	}

	// Slate로 붙여야 Travel에 유리
	SlateWidget = ActiveWidget->TakeWidget();
	ViewportClient->AddViewportWidgetContent(SlateWidget.ToSharedRef(), ScreenTransitionZOrder);

	State = EState::Covering;
	PendingOnCovered = MoveTemp(OnCovered);

	ActiveWidget->PlayCover(FSimpleDelegate::CreateUObject(this, &ThisClass::HandleCoverFinished), bInstant);
	return true;
}

void UScreenTransitionSubsystem::EndTransition()
{
	switch (State)
	{
	case EState::Covering:
		// 다 덮이면 바로 걷는다
		bRevealRequestedAfterCover = true;
		return;

	case EState::Covered:
		StartReveal();
		return;

	case EState::Idle:
	case EState::Revealing:
		return;
	}
}

bool UScreenTransitionSubsystem::IsTransitionActive() const
{
	return State != EState::Idle;
}

void UScreenTransitionSubsystem::HandleCoverFinished()
{
	State = EState::Covered;

	// 외부 콜백 전에 비운다. 콜백 안에서 EndTransition 이 다시 들어올 수 있다
	const bool bRevealAfterCover = bRevealRequestedAfterCover;
	bRevealRequestedAfterCover = false;

	FSimpleDelegate Local = MoveTemp(PendingOnCovered);
	PendingOnCovered.Unbind();
	Local.ExecuteIfBound();

	// 콜백이 이미 Reveal 을 시작했을 수 있다
	if (bRevealAfterCover && State == EState::Covered)
	{
		StartReveal();
	}
}

void UScreenTransitionSubsystem::StartReveal()
{
	if (!IsValid(ActiveWidget))
	{
		TearDownWidget();
		return;
	}

	State = EState::Revealing;
	ActiveWidget->PlayReveal(FSimpleDelegate::CreateUObject(this, &ThisClass::HandleRevealFinished));
}

void UScreenTransitionSubsystem::HandleRevealFinished()
{
	TearDownWidget();
}

void UScreenTransitionSubsystem::TearDownWidget()
{
	if (SlateWidget.IsValid())
	{
		if (UGameViewportClient* ViewportClient = GetGameViewportClient())
		{
			ViewportClient->RemoveViewportWidgetContent(SlateWidget.ToSharedRef());
		}
		SlateWidget.Reset();
	}

	ActiveWidget = nullptr;
	PendingOnCovered.Unbind();
	bRevealRequestedAfterCover = false;
	State = EState::Idle;
}

UGameViewportClient* UScreenTransitionSubsystem::GetGameViewportClient() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
}

#if !UE_BUILD_SHIPPING
namespace ScreenTransitionDebug
{
	UScreenTransitionSubsystem* FindTransitionSubsystem(const UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UScreenTransitionSubsystem>() : nullptr;
	}

	// 매치 없이 덮개만 검증한다. 인자 1 이면 즉시 덮는다
	FAutoConsoleCommandWithWorldAndArgs CoverCommand(
		TEXT("Shooter.Transition.Cover"),
		TEXT("화면 전환 덮개를 덮는다. 인자 1 이면 즉시"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World)
			{
				UScreenTransitionSubsystem* Transition = FindTransitionSubsystem(World);
				if (!Transition)
				{
					return;
				}

				const bool bInstant = Args.Num() > 0 && Args[0] == TEXT("1");
				const bool bStarted = Transition->BeginTransition(
					FSimpleDelegate::CreateLambda([]()
					{
						UE_LOG(LogScreenTransition, Display, TEXT("[Transition] Covered"));
					}),
					bInstant);

				UE_LOG(LogScreenTransition, Display, TEXT("[Transition] BeginTransition -> %s"),
					bStarted ? TEXT("true") : TEXT("false"));
			}));

	FAutoConsoleCommandWithWorld RevealCommand(
		TEXT("Shooter.Transition.Reveal"),
		TEXT("화면 전환 덮개를 걷는다"),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld* World)
			{
				if (UScreenTransitionSubsystem* Transition = FindTransitionSubsystem(World))
				{
					Transition->EndTransition();
				}
			}));
}
#endif
