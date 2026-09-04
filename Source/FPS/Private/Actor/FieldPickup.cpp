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
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;

	bReplicates = true;
	SetReplicateMovement(false);
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PresentationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PresentationRoot"));
	PresentationRoot->SetupAttachment(SceneRoot);

	PickupVisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PickupVisualRoot"));
	PickupVisualRoot->SetupAttachment(PresentationRoot);

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(PickupVisualRoot);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetGenerateOverlapEvents(false);
	PickupMesh->SetCanEverAffectNavigation(false);

	PedestalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PedestalMesh"));
	PedestalMesh->SetupAttachment(PresentationRoot);
	PedestalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PedestalMesh->SetGenerateOverlapEvents(false);
	PedestalMesh->SetCanEverAffectNavigation(false);

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

void AFieldPickup::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdatePresentationAnimation(DeltaSeconds);
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

	if (IsValid(PickupMesh))
	{
		PickupMeshBaseLocation = PickupMesh->GetRelativeLocation();
		PickupMeshBaseRotation = PickupMesh->GetRelativeRotation();
	}

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

	// 부유 중인 실제 픽업 위치에서 획득 연출이 재생되도록 한다.
	const FTransform EffectTransform = IsValid(PickupMesh)
		? PickupMesh->GetComponentTransform()
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
	// 재획득 시 제거할 Effect Tag를 가진 GE가 있으면 먼저 제거
	if (RemoveEffectsWithTagBeforeApply.IsValid())
	{
		TargetAbilitySystem.RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(
			FGameplayTagContainer(RemoveEffectsWithTagBeforeApply)));
	}

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
	const bool bCanRenderPresentation = GetNetMode() != NM_DedicatedServer;
	const bool bShowPickupVisual = bCanRenderPresentation && bIsAvailable;
	const bool bEnableServerPickup = HasAuthority()
		&& bIsAvailable
		&& HasValidConfiguration();

	// 소비/리스폰 상태는 실제 아이템 Mesh에만 적용한다.
	// 새 중간 SceneComponent의 부착 상태에 의존하지 않으므로 기존 BP도 그대로 동작한다.
	// PedestalMesh와 PresentationRoot 아래의 발판 연출은 항상 남아 있어야 한다.
	if (IsValid(PickupMesh))
	{
		PickupMesh->SetVisibility(bShowPickupVisual, true);
		PickupMesh->SetHiddenInGame(!bShowPickupVisual, true);
	}

	// 기존 AmbientNiagara는 픽업 본체의 가용 상태 연출로 유지한다.
	// 쿨타임에도 남길 발판 이펙트가 필요하면 BP에서 PresentationRoot 아래에 별도로 둔다.
	if (IsValid(AmbientNiagara))
	{
		if (bShowPickupVisual)
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

	if (bShowPickupVisual)
	{
		ResetPresentationAnimation();
	}

	// 부유/자전이 실제로 필요한 클라이언트에서만 Tick을 켠다.
	// 쿨타임, Dedicated Server, 모션 값이 0인 경우에는 Tick 비용이 없다.
	SetActorTickEnabled(ShouldAnimatePickup());
}

bool AFieldPickup::ShouldAnimatePickup() const
{
	if (!bIsAvailable
		|| GetNetMode() == NM_DedicatedServer
		|| !IsValid(PickupMesh))
	{
		return false;
	}

	const bool bHasHoverMotion = HoverAmplitude > KINDA_SMALL_NUMBER
		&& HoverFrequency > KINDA_SMALL_NUMBER;
	const bool bHasRotationMotion = !FMath::IsNearlyZero(RotationSpeed);

	return bHasHoverMotion || bHasRotationMotion;
}

void AFieldPickup::ResetPresentationAnimation()
{
	PresentationAnimationTime = 0.0f;

	if (!IsValid(PickupMesh))
	{
		return;
	}

	PickupMesh->SetRelativeLocation(PickupMeshBaseLocation);
	PickupMesh->SetRelativeRotation(PickupMeshBaseRotation);
}

void AFieldPickup::UpdatePresentationAnimation(const float DeltaSeconds)
{
	if (!bIsAvailable
		|| GetNetMode() == NM_DedicatedServer
		|| !IsValid(PickupMesh))
	{
		return;
	}

	PresentationAnimationTime += DeltaSeconds;

	const float HoverOffset = HoverAmplitude > 0.0f && HoverFrequency > 0.0f
		? FMath::Sin(PresentationAnimationTime * UE_TWO_PI * HoverFrequency) * HoverAmplitude
		: 0.0f;

	PickupMesh->SetRelativeLocation(
		PickupMeshBaseLocation + FVector(0.0f, 0.0f, HoverOffset));

	FRotator AnimatedRotation = PickupMeshBaseRotation;
	AnimatedRotation.Yaw += RotationSpeed * PresentationAnimationTime;
	PickupMesh->SetRelativeRotation(AnimatedRotation);
}
