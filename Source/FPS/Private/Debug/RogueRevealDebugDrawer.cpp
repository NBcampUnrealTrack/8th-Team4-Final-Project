// Fill out your copyright notice in the Description page of Project Settings.


#include "Debug/RogueRevealDebugDrawer.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Tags/GameplayTags.h"

namespace
{
	static TAutoConsoleVariable<int32> CVarDebugReveal(
		TEXT("Rogue.DebugReveal"),
		1,
		TEXT("발각된 캐릭터 위에 디버그 원을 그린다. 실루엣 렌더링이 붙으면 이 서브시스템을 삭제한다"),
		ECVF_Cheat);

	constexpr float SphereRadius = 30.f;
	constexpr int32 SphereSegments = 12;
	constexpr float HeightAboveHead = 40.f;
}

void URogueRevealDebugDrawer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if ENABLE_DRAW_DEBUG
	if (CVarDebugReveal.GetValueOnGameThread() == 0)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;

	if (!GameState)
	{
		return;
	}

	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		const APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}

		const UAbilitySystemComponent* ASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<APlayerState*>(PlayerState));

		if (!ASC || !ASC->HasMatchingGameplayTag(RogueTags::State_Combat_Revealed))
		{
			continue;
		}

		// 사망하면 넘어감
		if (ASC->HasMatchingGameplayTag(RogueTags::State_Combat_IsDead))
		{
			continue;
		}

		const FVector Center = Pawn->GetActorLocation()
			+ FVector(0.f, 0.f, Pawn->GetSimpleCollisionHalfHeight() + HeightAboveHead);

		// LifeTime -1 + 비영구라 이번 프레임만 남는다 — 태그가 떨어지면 즉시 사라진다
		// SDPG_Foreground 는 씬 뎁스를 무시하고 항상 위에 그린다. 드론이 벽을 통과해
		// 탐지하므로 표시도 벽 뒤에서 보여야 한다
		DrawDebugSphere(World, Center, SphereRadius, SphereSegments, FColor::Red,
			/*bPersistentLines*/ false, /*LifeTime*/ -1.f, SDPG_Foreground, /*Thickness*/ 2.f);
	}
#endif
}
