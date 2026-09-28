#include "Actor/DominationPointGimmick.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Actor/DominationCapturePoint.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/DominationGameMode.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "Player/BaseTeamPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationGimmick, Log, All);

ADominationPointGimmick::ADominationPointGimmick()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// 멀리 있는 플레이어도 마커를 볼 수 있어야 한다.
	bAlwaysRelevant = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->InitSphereRadius(175.0f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);
	InteractionSphere->SetCanEverAffectNavigation(false);
}

void ADominationPointGimmick::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		SetGimmickActive(false, nullptr);
	}

	Super::EndPlay(EndPlayReason);
}

void ADominationPointGimmick::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADominationPointGimmick, bGimmickActive);
	DOREPLIFETIME(ADominationPointGimmick, InteractingTeam);
	DOREPLIFETIME(ADominationPointGimmick, InteractionStartServerTime);
	DOREPLIFETIME(ADominationPointGimmick, OwningTeam);
	DOREPLIFETIME(ADominationPointGimmick, ReconTeam);
	DOREPLIFETIME(ADominationPointGimmick, ReconEndServerTime);
	DOREPLIFETIME(ADominationPointGimmick, CooldownEndServerTime);
}

void ADominationPointGimmick::SetGimmickActive(
	const bool bActive,
	const ADominationCapturePoint* OwnerPoint)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bActive)
	{
		ScanCenter = IsValid(OwnerPoint) ? OwnerPoint->GetActorLocation() : GetActorLocation();
		bGimmickActive = true;
		// 기믹 활성화 확인용 로그
		UE_LOG(LogDominationGimmick, Log, TEXT("[Gimmick] %s 활성화 (거점 %s)"),
			*GetNameSafe(this), *GetNameSafe(OwnerPoint));
		NotifyStateChanged();
		return;
	}

	if (!bGimmickActive
		&& InteractingTeam == EGameTeam::None
		&& OwningTeam == EGameTeam::None
		&& ReconTeam == EGameTeam::None)
	{
		return;
	}

	if (InteractingController.IsValid() || InteractingTeam != EGameTeam::None)
	{
		EndInteraction(false);
	}

	StopRecon();
	ClearRespawnRelay();
	CooldownEndServerTime = 0.0f;
	bGimmickActive = false;
	// 기믹 비활성화 확인용 로그
	UE_LOG(LogDominationGimmick, Log, TEXT("[Gimmick] %s 비활성화"), *GetNameSafe(this));
	NotifyStateChanged();
}

bool ADominationPointGimmick::CanBeUsedBy(const APawn* Pawn) const
{
	if (!bGimmickActive || InteractingTeam != EGameTeam::None || !IsValid(Pawn))
	{
		return false;
	}

	const ABaseTeamPlayerState* PlayerState = Pawn->GetPlayerState<ABaseTeamPlayerState>();
	if (!IsValid(PlayerState)
		|| !PlayerState->IsAlive()
		|| PlayerState->GetTeam() == EGameTeam::None
		|| !InteractionSphere->IsOverlappingActor(Pawn))
	{
		return false;
	}

	switch (GimmickType)
	{
	case EDominationGimmickType::Recon:
		return GetServerTime() >= CooldownEndServerTime;
	case EDominationGimmickType::RespawnRelay:
		return OwningTeam != PlayerState->GetTeam();
	default:
		return false;
	}
}

ADominationPointGimmick* ADominationPointGimmick::FindUsableGimmick(const APawn* Pawn)
{
	if (!IsValid(Pawn))
	{
		return nullptr;
	}

	TArray<AActor*> OverlappingGimmicks;
	Pawn->GetOverlappingActors(OverlappingGimmicks, ADominationPointGimmick::StaticClass());
	for (AActor* Actor : OverlappingGimmicks)
	{
		ADominationPointGimmick* Gimmick = Cast<ADominationPointGimmick>(Actor);
		if (IsValid(Gimmick) && Gimmick->CanBeUsedBy(Pawn))
		{
			return Gimmick;
		}
	}

	return nullptr;
}

bool ADominationPointGimmick::BeginInteraction(APlayerController* Controller)
{
	UWorld* World = GetWorld();
	APawn* Pawn = IsValid(Controller) ? Controller->GetPawn() : nullptr;
	if (!HasAuthority() || !IsValid(World) || !CanBeUsedBy(Pawn))
	{
		return false;
	}

	const ABaseTeamPlayerState* PlayerState = Pawn->GetPlayerState<ABaseTeamPlayerState>();
	InteractingController = Controller;
	InteractingTeam = PlayerState->GetTeam();
	InteractionStartServerTime = GetServerTime();
	World->GetTimerManager().SetTimer(
		InteractionTickHandle,
		this,
		&ThisClass::HandleInteractionTick,
		InteractionTickInterval,
		true);
	NotifyStateChanged();
	return true;
}

void ADominationPointGimmick::CancelInteraction(APlayerController* Controller)
{
	if (HasAuthority()
		&& InteractingController.IsValid()
		&& InteractingController.Get() == Controller)
	{
		EndInteraction(false);
	}
}

float ADominationPointGimmick::GetInteractionProgress() const
{
	if (InteractingTeam == EGameTeam::None || HoldDuration <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp((GetServerTime() - InteractionStartServerTime) / HoldDuration, 0.0f, 1.0f);
}

float ADominationPointGimmick::GetCooldownRemaining() const
{
	return FMath::Max(0.0f, CooldownEndServerTime - GetServerTime());
}

void ADominationPointGimmick::OnRep_GimmickState()
{
	BP_OnGimmickStateChanged();
}

void ADominationPointGimmick::MulticastInteractionEnded_Implementation(
	const bool bCompleted,
	const EGameTeam Team)
{
	BP_OnInteractionEnded(bCompleted, Team);
}

void ADominationPointGimmick::HandleInteractionTick()
{
	APlayerController* Controller = InteractingController.Get();
	APawn* Pawn = IsValid(Controller) ? Controller->GetPawn() : nullptr;
	const ABaseTeamPlayerState* PlayerState = IsValid(Pawn)
		? Pawn->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;

	// 사망, 범위 이탈, 컨트롤러 소실 시 진행률은 0으로 돌아간다. 피격만으로는 끊기지 않는다.
	if (!IsValid(PlayerState)
		|| !PlayerState->IsAlive()
		|| !InteractionSphere->IsOverlappingActor(Pawn))
	{
		EndInteraction(false);
		return;
	}

	if (GetServerTime() - InteractionStartServerTime >= HoldDuration)
	{
		EndInteraction(true);
	}
}

void ADominationPointGimmick::EndInteraction(const bool bCompleted)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InteractionTickHandle);
	}

	APlayerController* Controller = InteractingController.Get();
	const EGameTeam Team = InteractingTeam;
	InteractingController.Reset();
	InteractingTeam = EGameTeam::None;
	InteractionStartServerTime = 0.0f;

	if (bCompleted && Team != EGameTeam::None)
	{
		switch (GimmickType)
		{
		case EDominationGimmickType::Recon:
			ApplyRecon(Team);
			break;
		case EDominationGimmickType::RespawnRelay:
			ApplyRespawnRelay(Team);
			break;
		default:
			break;
		}
	}

	OnInteractionEnded.Broadcast(Controller, bCompleted);
	MulticastInteractionEnded(bCompleted, Team);
	NotifyStateChanged();
}

void ADominationPointGimmick::ApplyRecon(const EGameTeam Team)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (!RevealEffectClass)
	{
		UE_LOG(LogDominationGimmick, Warning,
			TEXT("%s has no RevealEffectClass. Recon scan will not reveal anyone."),
			*GetNameSafe(this));
	}

	const float Now = GetServerTime();
	ReconTeam = Team;
	CooldownEndServerTime = Now + ReconCooldown;
	ReconEndServerTime = Now + ReconScanCount * ReconScanInterval;
	ScansRemaining = ReconScanCount;

	RunReconScan();
	if (ScansRemaining > 0)
	{
		World->GetTimerManager().SetTimer(
			ReconScanHandle,
			this,
			&ThisClass::RunReconScan,
			ReconScanInterval,
			true);
	}
}

void ADominationPointGimmick::RunReconScan()
{
	UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	if (!IsValid(GameState) || ReconTeam == EGameTeam::None)
	{
		StopRecon();
		return;
	}

	const float RadiusSquared = FMath::Square(ReconRadius);
	for (APlayerState* BasePlayerState : GameState->PlayerArray)
	{
		ABaseTeamPlayerState* PlayerState = Cast<ABaseTeamPlayerState>(BasePlayerState);
		const APawn* Pawn = IsValid(PlayerState) ? PlayerState->GetPawn() : nullptr;
		if (!IsValid(Pawn)
			|| !PlayerState->IsAlive()
			|| PlayerState->GetTeam() == EGameTeam::None
			|| PlayerState->GetTeam() == ReconTeam
			|| FVector::DistSquared(Pawn->GetActorLocation(), ScanCenter) > RadiusSquared)
		{
			continue;
		}

		UAbilitySystemComponent* AbilitySystem =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState);
		if (IsValid(AbilitySystem) && RevealEffectClass)
		{
			AbilitySystem->ApplyGameplayEffectToSelf(
				RevealEffectClass->GetDefaultObject<UGameplayEffect>(),
				1.0f,
				AbilitySystem->MakeEffectContext());
		}
	}

	--ScansRemaining;
	if (ScansRemaining <= 0)
	{
		StopRecon();
	}
}

void ADominationPointGimmick::StopRecon()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReconScanHandle);
	}

	ScansRemaining = 0;
	if (ReconTeam != EGameTeam::None)
	{
		ReconTeam = EGameTeam::None;
		ReconEndServerTime = 0.0f;
		NotifyStateChanged();
	}
}

void ADominationPointGimmick::ApplyRespawnRelay(const EGameTeam Team)
{
	ADominationGameMode* GameMode = IsValid(GetWorld())
		? GetWorld()->GetAuthGameMode<ADominationGameMode>()
		: nullptr;
	if (!IsValid(GameMode))
	{
		return;
	}

	if (OwningTeam != EGameTeam::None)
	{
		GameMode->SetTeamRespawnDelayReduction(OwningTeam, 0.0f);
	}

	GameMode->SetTeamRespawnDelayReduction(Team, RespawnDelayReduction);
	OwningTeam = Team;
}

void ADominationPointGimmick::ClearRespawnRelay()
{
	if (OwningTeam == EGameTeam::None)
	{
		return;
	}

	if (ADominationGameMode* GameMode = IsValid(GetWorld())
		? GetWorld()->GetAuthGameMode<ADominationGameMode>()
		: nullptr)
	{
		GameMode->SetTeamRespawnDelayReduction(OwningTeam, 0.0f);
	}

	OwningTeam = EGameTeam::None;
}

void ADominationPointGimmick::NotifyStateChanged()
{
	// 서버는 OnRep이 오지 않으므로 직접 호출한다.
	BP_OnGimmickStateChanged();
}

float ADominationPointGimmick::GetServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	return IsValid(GameState) ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : 0.0f;
}
