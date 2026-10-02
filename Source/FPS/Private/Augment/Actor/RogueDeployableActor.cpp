// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/Actor/RogueDeployableActor.h"

#include "AbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "GAS/RogueHealthComponent.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Player/BaseTeamPlayerState.h"
#include "Tags/GameplayTags.h"

namespace
{
	// ShooterCharacter 의 팀 윤곽선 스텐실과 같은 값
	constexpr int32 DeployableOutlineStencilAlly = 1;
	constexpr int32 DeployableOutlineStencilEnemy = 2;
	constexpr int32 DeployableOutlineStencilEnemyRevealed = 3;
}

ARogueDeployableActor::ARogueDeployableActor()
{
	AbilitySystemComponent = CreateDefaultSubobject<URogueAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	VitalSet               = CreateDefaultSubobject<URogueVitalAttributeSet>(TEXT("VitalSet"));
	HealthComponent        = CreateDefaultSubobject<URogueHealthComponent>(TEXT("HealthComponent"));

	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	bAutoDetonate = false;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

UAbilitySystemComponent* ARogueDeployableActor::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float ARogueDeployableActor::GetHealth() const
{
	check(VitalSet);
	return VitalSet->GetHealth();
}

float ARogueDeployableActor::GetMaxHealth() const
{
	check(VitalSet);
	return VitalSet->GetMaxHealth();
}

void ARogueDeployableActor::BeginPlay()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	HealthComponent->InitializeWithAbilitySystem(AbilitySystemComponent);

	if (HasAuthority())
	{
		// PS를 Owner로 설정
		if (const APawn* InstigatorPawn = GetInstigator(); IsValid(InstigatorPawn))
		{
			if (APlayerState* CasterState = InstigatorPawn->GetPlayerState())
			{
				SetOwner(CasterState);
			}
		}

		AbilitySystemComponent->SetLooseGameplayTagCount(RogueTags::State_Combat_Deployable, 1);

		const float Health = FMath::Max(1.f, DeployHealth);
		AbilitySystemComponent->SetNumericAttributeBase(URogueVitalAttributeSet::GetMaxHealthAttribute(), Health);
		AbilitySystemComponent->SetNumericAttributeBase(URogueVitalAttributeSet::GetHealthAttribute(), Health);
	}

	// 이동은 예측
	MoveDirection = bMoveHorizontally
		? GetActorForwardVector().GetSafeNormal2D()
		: GetActorForwardVector();

	SetActorTickEnabled(MoveSpeed > 0.f && !MoveDirection.IsNearlyZero());

	Super::BeginPlay();

	if (GetNetMode() != NM_DedicatedServer)
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(RogueTags::State_Combat_Revealed, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleRevealedChanged);
		RefreshTeamOutline();
	}
}

void ARogueDeployableActor::OnRep_Owner()
{
	Super::OnRep_Owner();
	RefreshTeamOutline();
}

void ARogueDeployableActor::HandleRevealedChanged(const FGameplayTag, const int32)
{
	RefreshTeamOutline();
}

void ARogueDeployableActor::RefreshTeamOutline()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const APlayerController* ViewerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	const ABaseTeamPlayerState* ViewerState = IsValid(ViewerController)
		? ViewerController->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;

	// 서버가 Owner 를 시전자 PlayerState 로 바꾸므로 시전자가 죽어도 팀이 유지된다
	const ABaseTeamPlayerState* OwnerState = Cast<ABaseTeamPlayerState>(GetOwner());
	if (!IsValid(OwnerState))
	{
		const APawn* InstigatorPawn = GetInstigator();
		OwnerState = IsValid(InstigatorPawn) ? InstigatorPawn->GetPlayerState<ABaseTeamPlayerState>() : nullptr;
	}

	const bool bHasTeams = IsValid(ViewerState) && IsValid(OwnerState)
		&& ViewerState->GetTeam() != EGameTeam::None
		&& OwnerState->GetTeam() != EGameTeam::None;

	int32 StencilValue = DeployableOutlineStencilAlly;
	if (bHasTeams && ViewerState->GetTeam() != OwnerState->GetTeam())
	{
		StencilValue = AbilitySystemComponent->HasMatchingGameplayTag(RogueTags::State_Combat_Revealed)
			? DeployableOutlineStencilEnemyRevealed
			: DeployableOutlineStencilEnemy;
	}

	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->SetCustomDepthStencilValue(StencilValue);
		Primitive->SetRenderCustomDepth(bHasTeams);
	}
}

void ARogueDeployableActor::RefreshTeamOutlineForAll(UWorld* World)
{
	if (!IsValid(World))
	{
		return;
	}

	for (TActorIterator<ARogueDeployableActor> Iterator(World); Iterator; ++Iterator)
	{
		Iterator->RefreshTeamOutline();
	}
}

void ARogueDeployableActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FHitResult Hit;
	AddActorWorldOffset(MoveDirection * MoveSpeed * DeltaSeconds, /*bSweep*/ true, &Hit);

	// 벽에 닿으면 멈춤
	if (Hit.bBlockingHit)
	{
		SetActorTickEnabled(false);
	}
}

void ARogueDeployableActor::HandleDeath_Implementation(const FRogueDeathContext& Context)
{
	if (!HasAuthority() || bDestroyHandled)
	{
		return;
	}
	bDestroyHandled = true;

	if (DestroyedCueTag.IsValid())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location     = GetActorLocation();
		CueParams.Instigator   = Context.InstigatorActor;
		CueParams.EffectCauser = this;

		AbilitySystemComponent->ExecuteGameplayCue(DestroyedCueTag, CueParams);
	}

	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetLifeSpan(FMath::Max(KINDA_SMALL_NUMBER, DestroyDelaySeconds));
}
