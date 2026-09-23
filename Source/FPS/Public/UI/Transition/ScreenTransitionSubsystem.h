// ScreenTransitionSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ScreenTransitionSubsystem.generated.h"

class SWidget;
class UGameViewportClient;
class UScreenTransitionWidget;

// travel 트랜지션
UCLASS()
class FPS_API UScreenTransitionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// true 면 이 호출의 OnCovered 가 반드시 불리고, false 면 절대 안 불린다
	bool BeginTransition(FSimpleDelegate OnCovered = FSimpleDelegate(), bool bInstant = false);
	void EndTransition();
	bool IsTransitionActive() const;

private:
	enum class EState : uint8
	{
		Idle,
		Covering,
		Covered,
		Revealing
	};

	void HandleCoverFinished();
	void StartReveal();
	void HandleRevealFinished();
	void TearDownWidget();

	UGameViewportClient* GetGameViewportClient() const;

	EState State = EState::Idle;

	// Covering 중에 EndTransition 이 왔다
	bool bRevealRequestedAfterCover = false;

	UPROPERTY(Transient)
	TSubclassOf<UScreenTransitionWidget> ResolvedWidgetClass;
	UPROPERTY(Transient)
	TObjectPtr<UScreenTransitionWidget> ActiveWidget;

	// 뷰포트에 직접 붙인 Slate 본체. 떼는 것도 이 참조로 한다
	TSharedPtr<SWidget> SlateWidget;

	FSimpleDelegate PendingOnCovered;
};
