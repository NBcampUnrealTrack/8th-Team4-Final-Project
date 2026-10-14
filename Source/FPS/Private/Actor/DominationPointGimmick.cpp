#include "Actor/DominationPointGimmick.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Actor/DominationCapturePoint.h"
#include "Audio/DominationSystemAnnouncer.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameMode/DominationGameMode.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
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
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(
		this,
		&ThisClass::HandleInteractionSphereOverlapChanged);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(
		this,
		&ThisClass::HandleInteractionSphereEndOverlap);
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
	DOREPLIFETIME(ADominationPointGimmick, InteractingPlayerState);
	DOREPLIFETIME(ADominationPointGimmick, OwningTeam);
	DOREPLIFETIME(ADominationPointGimmick, ReconTeam);
	DOREPLIFETIME(ADominationPointGimmick, ReconEndServerTime);
	DOREPLIFETIME(ADominationPointGimmick, CooldownEndServerTime);
	DOREPLIFETIME(ADominationPointGimmick, ScanCenter);
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
	InteractingPlayerState = Controller->PlayerState;
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

float ADominationPointGimmick::GetReconTimeRemaining() const
{
	return ReconTeam != EGameTeam::None
		? FMath::Max(0.0f, ReconEndServerTime - GetServerTime())
		: 0.0f;
}

APlayerState* ADominationPointGimmick::GetInteractingPlayerState() const
{
	return InteractingPlayerState;
}

ADominationPointGimmick* ADominationPointGimmick::GetActiveGimmick(const UObject* WorldContextObject)
{
	UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return nullptr;
	}

	// 활성 거점은 하나뿐이므로 켜진 기믹도 최대 하나다.
	for (TActorIterator<ADominationPointGimmick> Iterator(World); Iterator; ++Iterator)
	{
		if (Iterator->IsGimmickActive())
		{
			return *Iterator;
		}
	}

	return nullptr;
}

void ADominationPointGimmick::OnRep_GimmickState()
{
	NotifyStateChanged();
}

void ADominationPointGimmick::HandleInteractionSphereOverlapChanged(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32,
	bool,
	const FHitResult&)
{
	if (bGimmickActive && IsValid(Cast<APawn>(OtherActor)))
	{
		OnGimmickStateChanged.Broadcast(this);
	}
}

void ADominationPointGimmick::HandleInteractionSphereEndOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32)
{
	if (bGimmickActive && IsValid(Cast<APawn>(OtherActor)))
	{
		OnGimmickStateChanged.Broadcast(this);
	}
}

void ADominationPointGimmick::MulticastInteractionEnded_Implementation(
	const bool bCompleted,
	const EGameTeam Team,
	const bool bTakeover)
{
	PlayInteractionEndedCosmetics(bCompleted);
	BP_OnInteractionEnded(bCompleted, Team);
	if (bCompleted)
	{
		PlaySecuredAnnouncement(Team, bTakeover);
		OnGimmickSecured.Broadcast(this, Team, bTakeover);
	}
}

bool ADominationPointGimmick::ShouldPlayCosmetics() const
{
	return GetNetMode() != NM_DedicatedServer;
}

void ADominationPointGimmick::PlayInteractionStartedCosmetics()
{
	if (!ShouldPlayCosmetics())
	{
		return;
	}

	if (IsValid(InteractionStartSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, InteractionStartSound, GetActorLocation());
	}

	StopInteractionLoopSound();
	if (IsValid(InteractionLoopSound))
	{
		InteractionLoopAudio = UGameplayStatics::SpawnSoundAttached(InteractionLoopSound, GetRootComponent());
	}
}

void ADominationPointGimmick::PlayInteractionEndedCosmetics(const bool bCompleted)
{
	if (!ShouldPlayCosmetics())
	{
		return;
	}

	StopInteractionLoopSound();
	if (bCompleted && IsValid(InteractionCompleteSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, InteractionCompleteSound, GetActorLocation());
	}

	// 완료 신호와 같은 시점에 재생하므로 늦게 접속한 클라이언트에서는 과거 확보 이펙트가 재생되지 않는다.
	if (bCompleted
		&& GimmickType == EDominationGimmickType::RespawnRelay
		&& IsValid(RelaySecuredEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, RelaySecuredEffect, GetActorLocation());
	}
}

void ADominationPointGimmick::PlaySecuredAnnouncement(const EGameTeam SecuringTeam, const bool bTakeover)
{
	UWorld* World = GetWorld();
	const APlayerController* LocalController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	const ABaseTeamPlayerState* LocalPlayerState = IsValid(LocalController)
		? LocalController->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;
	if (!ShouldPlayCosmetics() || !IsValid(LocalPlayerState) || LocalPlayerState->GetTeam() == EGameTeam::None)
	{
		return;
	}

	const bool bAlly = LocalPlayerState->GetTeam() == SecuringTeam;
	FName AnnouncementId;
	if (bTakeover)
	{
		AnnouncementId = bAlly
			? DominationAnnouncementIds::GimmickRelayTakeoverAlly
			: DominationAnnouncementIds::GimmickRelayTakeoverEnemy;
	}
	else if (GimmickType == EDominationGimmickType::RespawnRelay)
	{
		AnnouncementId = bAlly
			? DominationAnnouncementIds::GimmickRelaySecuredAlly
			: DominationAnnouncementIds::GimmickRelaySecuredEnemy;
	}
	else
	{
		AnnouncementId = bAlly
			? DominationAnnouncementIds::GimmickReconSecuredAlly
			: DominationAnnouncementIds::GimmickReconSecuredEnemy;
	}

	// 방송기는 맵에 하나뿐이다. 없거나 해당 ID의 음성을 등록하지 않았으면 건너뛴다.
	for (TActorIterator<ASystemAnnouncer> Iterator(World); Iterator; ++Iterator)
	{
		Iterator->AnnounceLocal(AnnouncementId);
		break;
	}
}

void ADominationPointGimmick::PlayReconStartedCosmetics()
{
	if (!ShouldPlayCosmetics() || !IsValid(ReconScanEffect))
	{
		return;
	}

	if (UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ReconScanEffect, ScanCenter))
	{
		Effect->SetVariableFloat(TEXT("User.Radius"), ReconRadius);
	}
}

void ADominationPointGimmick::StopInteractionLoopSound()
{
	if (IsValid(InteractionLoopAudio))
	{
		InteractionLoopAudio->FadeOut(InteractionLoopFadeOutDuration, 0.0f);
	}

	InteractionLoopAudio = nullptr;
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
	InteractingPlayerState = nullptr;
	InteractionStartServerTime = 0.0f;

	// 소유 팀이 바뀌기 전에 탈취 여부를 정한다(효과 적용이 OwningTeam을 덮어쓰기 때문).
	const bool bTakeover = bCompleted
		&& GimmickType == EDominationGimmickType::RespawnRelay
		&& OwningTeam != EGameTeam::None
		&& OwningTeam != Team;

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
	MulticastInteractionEnded(bCompleted, Team, bTakeover);
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
	// 서버는 OnRep이 오지 않으므로 직접 호출한다. 클라에서는 OnRep에서 호출된다.
	BroadcastStateTransitions();
	OnGimmickStateChanged.Broadcast(this);
}

void ADominationPointGimmick::BroadcastStateTransitions()
{
	// 켜짐은 다른 전환보다 먼저, 꺼짐은 나중에 알려 이펙트 순서를 자연스럽게 맞춘다.
	if (bGimmickActive && !bNotifiedActive)
	{
		bNotifiedActive = true;
		BP_OnActivated();
	}

	if (InteractingTeam != NotifiedInteractingTeam)
	{
		NotifiedInteractingTeam = InteractingTeam;
		if (InteractingTeam != EGameTeam::None)
		{
			PlayInteractionStartedCosmetics();
			BP_OnInteractionStarted(InteractingTeam);
		}
		else
		{
			// 종료 사운드는 MulticastInteractionEnded에서 재생한다. 여기서는 루프만 확실히 끈다.
			StopInteractionLoopSound();
		}
	}

	if (ReconTeam != NotifiedReconTeam)
	{
		const bool bWasScanning = NotifiedReconTeam != EGameTeam::None;
		NotifiedReconTeam = ReconTeam;
		if (bWasScanning)
		{
			BP_OnReconEnded();
		}

		if (ReconTeam != EGameTeam::None)
		{
			PlayReconStartedCosmetics();
			BP_OnReconStarted(ReconTeam);
		}
	}

	if (OwningTeam != NotifiedOwningTeam)
	{
		NotifiedOwningTeam = OwningTeam;
		BP_OnOwnerChanged(OwningTeam);
	}

	if (!bGimmickActive && bNotifiedActive)
	{
		bNotifiedActive = false;
		StopInteractionLoopSound();
		BP_OnDeactivated();
	}
}

float ADominationPointGimmick::GetServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	return IsValid(GameState) ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : 0.0f;
}
