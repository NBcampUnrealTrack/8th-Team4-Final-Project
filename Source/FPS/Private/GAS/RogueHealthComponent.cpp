// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/RogueHealthComponent.h"

#include "AbilitySystemComponent.h"
#include "GAS/Damage/RogueGameplayEffectContext.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Interfaces/RogueDamageableTarget.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/DominationPlayerState.h"
#include "Settings/ShooterAudioSettings.h"
#include "Tags/GameplayTags.h"

URogueHealthComponent::URogueHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void URogueHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(URogueHealthComponent, bIsDead);
	DOREPLIFETIME(URogueHealthComponent, DeathReaction);

	// 피격 방향 인디케이터는 본인 화면에만 필요하므로 소유 클라이언트에만 복제한다.
	DOREPLIFETIME_CONDITION(URogueHealthComponent, HitReaction, COND_OwnerOnly);
}

void URogueHealthComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	// 이미 초기화된 ASC는 다시 초기화 X
	if (AbilitySystemComponent == InASC)
	{
		return;
	}

	UninitializeFromAbilitySystem();

	if (!ensure(InASC))
	{
		return;
	}

	URogueVitalAttributeSet* Set =
		const_cast<URogueVitalAttributeSet*>(InASC->GetSet<URogueVitalAttributeSet>());

	if (!ensureMsgf(Set, TEXT("[RogueHealth] %s: ASC 에 URogueVitalAttributeSet 이 없습니다."),
		*GetNameSafe(GetOwner())))
	{
		return;
	}

	AbilitySystemComponent = InASC;
	VitalSet               = Set;

	// 구독은 서버에서만
	// 클라이언트는 레플리케이션으로 태그를 추가
	if (InASC->IsOwnerActorAuthoritative())
	{
		OutOfHealthHandle = VitalSet->OnOutOfHealth.AddUObject(this, &URogueHealthComponent::HandleOutOfHealth);

		DamageTakenEventHandle = InASC->AddGameplayEventTagContainerDelegate(
			FGameplayTagContainer(RogueTags::Event_Combat_DamageTaken),
			FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &URogueHealthComponent::HandleDamageTakenEvent));
	}

	// 이미 사망 상태면 연출 재생
	if (bIsDead)
	{
		ApplyDeathStateLocally(true);
	}
}

void URogueHealthComponent::UninitializeFromAbilitySystem()
{
	if (VitalSet && OutOfHealthHandle.IsValid())
	{
		VitalSet->OnOutOfHealth.Remove(OutOfHealthHandle);
	}

	if (AbilitySystemComponent && DamageTakenEventHandle.IsValid())
	{
		AbilitySystemComponent->RemoveGameplayEventTagContainerDelegate(
			FGameplayTagContainer(RogueTags::Event_Combat_DamageTaken), DamageTakenEventHandle);
	}

	OutOfHealthHandle.Reset();
	DamageTakenEventHandle.Reset();
	VitalSet               = nullptr;
	AbilitySystemComponent = nullptr;
}

void URogueHealthComponent::ServerResetDeathState()
{
	const AActor* Owner = GetOwner();
	if (!ensure(Owner && Owner->HasAuthority()))
	{
		return;
	}

	// 데미지 처리를 위한 AttributeSet의 플래그 초기화
	if (ensureMsgf(VitalSet, TEXT("[RogueHealth] %s: VitalSet 이 없어 사망 플래그를 지우지 못했습니다"),
		*GetNameSafe(Owner)))
	{
		VitalSet->ClearDeathDeclared();
	}

	if (!bIsDead)
	{
		return;
	}

	bIsDead = false;

	// 서버에서는 직접 반영
	ApplyDeathStateLocally(false);
	OnDeathStateChanged.Broadcast(false);
}

void URogueHealthComponent::OnUnregister()
{
	UninitializeFromAbilitySystem();

	Super::OnUnregister();
}

void URogueHealthComponent::OnRep_bIsDead()
{
	ApplyDeathStateLocally(bIsDead);
	OnDeathStateChanged.Broadcast(bIsDead);
}

void URogueHealthComponent::OnRep_HitReaction()
{
	OnHitReactionReceived.Broadcast(HitReaction.Direction);
}

FVector URogueHealthComponent::ComputeHitDirection(const FGameplayEffectContextHandle& ContextHandle, const AActor* InstigatorActor) const
{
	const AActor* Victim = AbilitySystemComponent ? AbilitySystemComponent->GetAvatarActor() : nullptr;
	if (!Victim)
	{
		return FVector::ZeroVector;
	}

	// ServerBuildDeathReaction의 방향(가해자->피격자, 랙돌 임펄스용)과 반대 부호다 - 인디케이터는
	// "피격자 -> 가해자" 방향(어느 쪽을 가리켜야 하는지)이 필요하다.
	if (const FRogueGameplayEffectContext* Ctx = FRogueGameplayEffectContext::ExtractFrom(ContextHandle))
	{
		const FVector ShotDirection = Ctx->HitShotDirection.GetSafeNormal2D();
		if (!ShotDirection.IsZero())
		{
			return -ShotDirection;
		}
	}

	if (IsValid(InstigatorActor) && InstigatorActor != Victim)
	{
		return (InstigatorActor->GetActorLocation() - Victim->GetActorLocation()).GetSafeNormal2D();
	}

	return FVector::ZeroVector;
}

void URogueHealthComponent::HandleDamageTakenEvent(const FGameplayTag EventTag, const FGameplayEventData* Payload)
{
	if (!Payload)
	{
		return;
	}

	// 고통 사운드는 피격 방향을 못 구해도(자해/낙사 등) 재생돼야 하므로 아래 방향 계산과 무관하게 먼저 처리.
	PlayPainSoundIfDue();

	const AActor* InstigatorActor = Payload->Instigator;
	const FVector Direction = ComputeHitDirection(Payload->ContextHandle, InstigatorActor);
	OnDamageTakenNative.Broadcast(InstigatorActor, Direction);

	if (Direction.IsZero())
	{
		return;
	}

	HitReaction.Direction = Direction;
	HitReaction.HitSequence++;

	// 서버(리슨서버 로컬 플레이어) 자신에게는 OnRep이 호출되지 않으므로 직접 브로드캐스트한다 -
	// ApplyDeathStateLocally + OnDeathStateChanged.Broadcast 조합과 같은 이유.
	OnHitReactionReceived.Broadcast(Direction);
}

void URogueHealthComponent::PlayPainSoundIfDue()
{
	AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!IsValid(Owner) || !World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	if (Now - LastPainSoundTime < PainSoundCooldownSeconds)
	{
		// 순식간에 여러 발 맞은 경우(펠릿, 동시 피격 등) - 최초 사운드만 유지하고 스킵한다.
		return;
	}

	const UShooterAudioSettings* AudioSettings = GetDefault<UShooterAudioSettings>();
	if (!AudioSettings)
	{
		return;
	}

	const EDominationCombatClass CombatClass = ResolveOwnerCombatClass();
	const TArray<TSoftObjectPtr<USoundBase>>& Pool = AudioSettings->ResolvePainSoundPool(CombatClass);
	if (Pool.IsEmpty())
	{
		return;
	}

	LastPainSoundTime = Now;

	// 플레이어는 이 컴포넌트가 PlayerState에 붙어 있어 Owner 위치가 캐릭터가 아니다(월드 원점 근처).
	// 실제로 맞은 몸(ASC 아바타)에서 소리가 나야 한다.
	const AActor* Avatar = AbilitySystemComponent ? AbilitySystemComponent->GetAvatarActor() : nullptr;
	const FVector SoundLocation = IsValid(Avatar) ? Avatar->GetActorLocation() : Owner->GetActorLocation();

	// 재생할 배리에이션은 서버가 한 번만 골라서 모든 클라이언트가 같은 사운드를 듣게 한다.
	const int32 SoundIndex = FMath::RandHelper(Pool.Num());
	Multicast_PlayPainSound(CombatClass, SoundIndex, SoundLocation);
}

EDominationCombatClass URogueHealthComponent::ResolveOwnerCombatClass() const
{
	// 플레이어는 PlayerState에, 훈련장 캐릭터 등은 Pawn 자신에 이 컴포넌트가 붙는다. 둘 다 처리해야
	// 클래스별 고통음 풀을 찾는다 - Pawn만 보면 플레이어는 항상 Unselected(빈 Default 풀)가 되어 무음.
	const ADominationPlayerState* PlayerState = Cast<ADominationPlayerState>(GetOwner());
	if (!IsValid(PlayerState))
	{
		if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			PlayerState = OwnerPawn->GetPlayerState<ADominationPlayerState>();
		}
	}

	return IsValid(PlayerState) ? PlayerState->GetCombatClass() : EDominationCombatClass::Unselected;
}

void URogueHealthComponent::Multicast_PlayPainSound_Implementation(
	const EDominationCombatClass CombatClass, const int32 SoundIndex, const FVector Location)
{
	const UShooterAudioSettings* AudioSettings = GetDefault<UShooterAudioSettings>();
	if (!AudioSettings)
	{
		return;
	}

	const TArray<TSoftObjectPtr<USoundBase>>& Pool = AudioSettings->ResolvePainSoundPool(CombatClass);
	if (!Pool.IsValidIndex(SoundIndex))
	{
		return;
	}

	if (USoundBase* Sound = Pool[SoundIndex].LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location);
	}
}

void URogueHealthComponent::HandleOutOfHealth(UAbilitySystemComponent* VictimASC,
                                              const FGameplayEffectContextHandle& ContextHandle)
{
	if (!ensure(VictimASC && VictimASC == AbilitySystemComponent))
	{
		return;
	}

	if (bIsDead)
	{
		return;
	}

	// 사망 연출용 정보 구성
	ServerBuildDeathReaction(ContextHandle);

	bIsDead = true;

	// 서버에서는 직접 반영
	ApplyDeathStateLocally(true);
	OnDeathStateChanged.Broadcast(true);

	NotifyDeath(ContextHandle);
}

void URogueHealthComponent::ApplyDeathStateLocally(bool bDead)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// 사망 태그 추가
	AbilitySystemComponent->SetLooseGameplayTagCount(RogueTags::State_Combat_IsDead, bDead ? 1 : 0);

	// 연출은 단방향. 원복은 폰 재스폰이 담당
	if (!bDead)
	{
		return;
	}

	AActor* Avatar = AbilitySystemComponent->GetAvatarActor();
	if (Avatar && Avatar->Implements<URogueDamageableTarget>())
	{
		IRogueDamageableTarget::Execute_PlayDeathReaction(Avatar, DeathReaction);
	}
}

void URogueHealthComponent::ServerBuildDeathReaction(const FGameplayEffectContextHandle& ContextHandle)
{
	DeathReaction = FRogueDeathReaction();

	const AActor* Victim = AbilitySystemComponent ? AbilitySystemComponent->GetAvatarActor() : nullptr;
	if (!Victim)
	{
		return;
	}

	// 히트스캔 사망 연출 정보 구성
	if (const FRogueGameplayEffectContext* Ctx = FRogueGameplayEffectContext::ExtractFrom(ContextHandle))
	{
		DeathReaction.bHeadshot   = Ctx->bHeadshot;
		DeathReaction.HitBoneName = Ctx->HitBoneName;

		// 수평 방향만 사용
		DeathReaction.Direction   = Ctx->HitShotDirection.GetSafeNormal2D();
	}

	// 히트스캔 종료
	if (!DeathReaction.Direction.IsZero())
	{
		return;
	}

	// 히트스캔이 아닌경우 방향만 계산
	const AActor* Instigator = ContextHandle.GetOriginalInstigator();
	if (IsValid(Instigator) && Instigator != Victim)
	{
		DeathReaction.Direction =
			(Victim->GetActorLocation() - Instigator->GetActorLocation()).GetSafeNormal2D();
	}
}

void URogueHealthComponent::NotifyDeath(const FGameplayEffectContextHandle& ContextHandle) const
{
	AActor* Victim = AbilitySystemComponent ? AbilitySystemComponent->GetAvatarActor() : nullptr;
	if (!Victim)
	{
		return;
	}

	if (!ensureMsgf(Victim->Implements<URogueDamageableTarget>(),
		TEXT("[RogueHealth] %s 가 IRogueDamageableTarget 을 구현하지 않았습니다"), *GetNameSafe(Victim)))
	{
		return;
	}

	FRogueDeathContext DeathContext;
	DeathContext.VictimActor     = Victim;
	DeathContext.InstigatorActor = ContextHandle.GetOriginalInstigator();
	DeathContext.CauserActor     = ContextHandle.GetEffectCauser();

	if (const FRogueGameplayEffectContext* Ctx = FRogueGameplayEffectContext::ExtractFrom(ContextHandle))
	{
		DeathContext.WeaponTag = Ctx->WeaponTag;
		DeathContext.bHeadshot = Ctx->bHeadshot;
	}

	IRogueDamageableTarget::Execute_HandleDeath(Victim, DeathContext);
}
