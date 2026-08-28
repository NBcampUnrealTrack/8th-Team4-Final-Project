#include "Actor/FieldPickup.h"

#include "AbilitySystemComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystemInterface.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GAS/RogueTargetFilter.h"
#include "GameplayEffect.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Tags/GameplayTags.h"
#include "Templates/UnrealTemplate.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogFieldPickup, Log, All);

AFieldPickup::AFieldPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(false);
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PresentationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PresentationRoot"));
	PresentationRoot->SetupAttachment(SceneRoot);

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(PresentationRoot);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetGenerateOverlapEvents(false);
	PickupMesh->SetCanEverAffectNavigation(false);

	AmbientNiagara = CreateDefaultSubobject<UNiagaraComponent>(TEXT("AmbientNiagara"));
	AmbientNiagara->SetupAttachment(PresentationRoot);
	AmbientNiagara->SetAutoActivate(false);
	AmbientNiagara->SetCanEverAffectNavigation(false);

	PickupTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("PickupTrigger"));
	PickupTrigger->SetupAttachment(SceneRoot);
	PickupTrigger->InitSphereRadius(100.0f);
	PickupTrigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	PickupTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupTrigger->SetGenerateOverlapEvents(false);
	PickupTrigger->SetCanEverAffectNavigation(false);
	PickupTrigger->OnComponentBeginOverlap.AddDynamic(
		this,
		&ThisClass::HandlePickupTriggerBeginOverlap);
}

void AFieldPickup::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFieldPickup, bIsAvailable);
}

void AFieldPickup::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && !HasValidConfiguration())
	{
		UE_LOG(
			LogFieldPickup,
			Error,
			TEXT("[FieldPickup] %s: PickupGameplayEffect 또는 RespawnDelay 설정을 확인하세요. Effect=%s, RespawnDelay=%.2f"),
			*GetPathName(),
			*GetNameSafe(PickupGameplayEffect),
			RespawnDelay);

		// 잘못 설정된 픽업은 보이기만 하고 수거되지 않는 상태로 남기지 않는다.
		SetAvailable(false);
		return;
	}

	ApplyAvailabilityState();

	if (HasAuthority() && bIsAvailable)
	{
		GetWorldTimerManager().SetTimerForNextTick(
			this,
			&ThisClass::EvaluateOverlappingPawns);
	}
}

void AFieldPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);

	Super::EndPlay(EndPlayReason);
}

void AFieldPickup::HandlePickupTriggerBeginOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32,
	bool,
	const FHitResult&)
{
	TryConsume(OtherActor);
}

void AFieldPickup::OnRep_IsAvailable()
{
	ApplyAvailabilityState();
}

void AFieldPickup::MulticastPlayPickupEffects_Implementation()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const FTransform EffectTransform = IsValid(PresentationRoot)
		? PresentationRoot->GetComponentTransform()
		: GetActorTransform();

	if (IsValid(PickupSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			PickupSound,
			EffectTransform.GetLocation());
	}

	if (IsValid(PickupNiagara))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			PickupNiagara,
			EffectTransform.GetLocation(),
			EffectTransform.Rotator(),
			EffectTransform.GetScale3D());
	}
}

bool AFieldPickup::TryConsume(AActor* CandidateActor)
{
	if (!HasAuthority()
		|| !bIsAvailable
		|| bConsumeInProgress
		|| !HasValidConfiguration())
	{
		return false;
	}

	APawn* CandidatePawn = Cast<APawn>(CandidateActor);
	if (!IsValid(CandidatePawn)
		|| !CandidatePawn->IsPlayerControlled()
		|| !URogueTargetFilter::IsAlive(CandidatePawn))
	{
		return false;
	}

	IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(CandidatePawn);
	if (!AbilitySystemInterface)
	{
		return false;
	}

	UAbilitySystemComponent* TargetAbilitySystem = AbilitySystemInterface->GetAbilitySystemComponent();
	if (!IsValid(TargetAbilitySystem)
		|| !TargetAbilitySystem->IsOwnerActorAuthoritative()
		|| TargetAbilitySystem->GetAvatarActor() != CandidatePawn)
	{
		return false;
	}

	TGuardValue<bool> ConsumeGuard(bConsumeInProgress, true);
	if (!TryApplyGameplayEffect(*TargetAbilitySystem))
	{
		return false;
	}

	ConsumePickup();
	return true;
}

bool AFieldPickup::TryApplyGameplayEffect(
	UAbilitySystemComponent& TargetAbilitySystem) const
{
	FGameplayEffectContextHandle EffectContext = TargetAbilitySystem.MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = TargetAbilitySystem.MakeOutgoingSpec(
		PickupGameplayEffect,
		1.0f,
		EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		UE_LOG(
			LogFieldPickup,
			Warning,
			TEXT("[FieldPickup] %s: GameplayEffect Spec 생성에 실패했습니다."),
			*GetPathName());
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(
		RogueTags::Data_Magnitude_Primary,
		PickupMagnitude);

	const FActiveGameplayEffectHandle AppliedHandle =
		TargetAbilitySystem.ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	// Instant GE는 IsValid()가 false여도 정상 실행일 수 있다.
	// 적용 조건과 Application Requirement까지 통과했는지는 이 값을 기준으로 판정한다.
	return AppliedHandle.WasSuccessfullyApplied();
}

bool AFieldPickup::HasValidConfiguration() const
{
	return IsValid(PickupGameplayEffect)
		&& RespawnDelay > 0.0f;
}

void AFieldPickup::ConsumePickup()
{
	SetAvailable(false);
	MulticastPlayPickupEffects();

	GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
	GetWorldTimerManager().SetTimer(
		RespawnTimerHandle,
		this,
		&ThisClass::HandleRespawnTimerExpired,
		RespawnDelay,
		false);
}

void AFieldPickup::HandleRespawnTimerExpired()
{
	if (!HasAuthority())
	{
		return;
	}

	SetAvailable(true);

	// 리스폰 순간 이미 트리거 안에 서 있으면 BeginOverlap이 다시 오지 않을 수 있다.
	GetWorldTimerManager().SetTimerForNextTick(
		this,
		&ThisClass::EvaluateOverlappingPawns);
}

void AFieldPickup::EvaluateOverlappingPawns()
{
	if (!HasAuthority()
		|| !bIsAvailable
		|| !IsValid(PickupTrigger))
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	PickupTrigger->GetOverlappingActors(OverlappingActors, APawn::StaticClass());

	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (TryConsume(OverlappingActor))
		{
			return;
		}
	}
}

void AFieldPickup::SetAvailable(const bool bNewAvailable)
{
	if (!HasAuthority() || bIsAvailable == bNewAvailable)
	{
		return;
	}

	bIsAvailable = bNewAvailable;
	ApplyAvailabilityState();
	ForceNetUpdate();
}

void AFieldPickup::ApplyAvailabilityState()
{
	const bool bShowPresentation = bIsAvailable
		&& GetNetMode() != NM_DedicatedServer;
	const bool bEnableServerPickup = HasAuthority()
		&& bIsAvailable
		&& HasValidConfiguration();

	// BP에서 PresentationRoot 아래에 추가한 시각 컴포넌트도 같은 상태를 따른다.
	if (IsValid(PresentationRoot))
	{
		PresentationRoot->SetHiddenInGame(!bShowPresentation, true);
	}

	if (IsValid(AmbientNiagara))
	{
		if (bShowPresentation)
		{
			AmbientNiagara->Activate(true);
		}
		else
		{
			AmbientNiagara->Deactivate();
		}
	}

	if (IsValid(PickupTrigger))
	{
		PickupTrigger->SetGenerateOverlapEvents(bEnableServerPickup);
		PickupTrigger->SetCollisionEnabled(
			bEnableServerPickup
				? ECollisionEnabled::QueryOnly
				: ECollisionEnabled::NoCollision);
	}
}
