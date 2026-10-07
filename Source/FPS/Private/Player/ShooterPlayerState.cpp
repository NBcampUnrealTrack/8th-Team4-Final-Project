// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/ShooterPlayerState.h"
#include "Customization/GameplayAppearanceComponent.h"

#include "Augment/AugmentInventoryComponent.h"
#include "Augment/Actor/RogueDeployableActor.h"
#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "Economy/CreditWalletComponent.h"
#include "GAS/RogueAbilitySet.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "GAS/RogueAmmoAttributeSet.h"
#include "GAS/RogueGASSettings.h"
#include "GAS/RogueAugmentAttributeSet.h"
#include "GAS/RogueHealthComponent.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "GameMode/State/LobbyGameState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Tags/GameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Shop/ShopComponent.h"
#include "Shop/ShopPurchaseComponent.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/WeaponInventoryComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterPlayerState, Log, All);

AShooterPlayerState::AShooterPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<URogueAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	VitalSet   = CreateDefaultSubobject<URogueVitalAttributeSet>(TEXT("VitalSet"));
	AugmentSet = CreateDefaultSubobject<URogueAugmentAttributeSet>(TEXT("AugmentSet"));
	AmmoSet    = CreateDefaultSubobject<URogueAmmoAttributeSet>(TEXT("AmmoSet"));

	HealthComponent  = CreateDefaultSubobject<URogueHealthComponent>(TEXT("HealthComponent"));
	AugmentInventory = CreateDefaultSubobject<UAugmentInventoryComponent>(TEXT("AugmentInventory"));
	CreditWallet = CreateDefaultSubobject<UCreditWalletComponent>(TEXT("CreditWallet"));
	ShopComponent = CreateDefaultSubobject<UShopComponent>(TEXT("ShopComponent"));
	ShopPurchaseComponent = CreateDefaultSubobject<UShopPurchaseComponent>(TEXT("ShopPurchaseComponent"));
	WeaponInventory = CreateDefaultSubobject<UWeaponInventoryComponent>(TEXT("WeaponInventory"));

	// ASC, AttributeSet이 PlayerState 에 있어 업데이트 주기 수정
	SetNetUpdateFrequency(100.f);
}

void AShooterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterPlayerState, bIsLobbyReady);
    DOREPLIFETIME(AShooterPlayerState, SavedAppearances);
	DOREPLIFETIME(AShooterPlayerState, bLoadedIntoMatch);
	DOREPLIFETIME(AShooterPlayerState, ParticipationState);
	DOREPLIFETIME(AShooterPlayerState, bIsVoiceTalking);
	DOREPLIFETIME(AShooterPlayerState, Kills);
	DOREPLIFETIME(AShooterPlayerState, Deaths);
}

UAbilitySystemComponent* AShooterPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AShooterPlayerState::SyncEquippedWeaponState(const FGameplayTag EquippedStateTag)
{
	AbilitySystemComponent->SetEquippedWeaponType(EquippedStateTag);
	ClientSyncEquippedWeaponState(EquippedStateTag);
}

void AShooterPlayerState::ClientSyncEquippedWeaponState_Implementation(const FGameplayTag EquippedStateTag)
{
	AbilitySystemComponent->SetEquippedWeaponType(EquippedStateTag);
}

void AShooterPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 폰이 붙는 시점을 PlayerState에서 확인 가능
	OnPawnSet.AddDynamic(this, &AShooterPlayerState::HandlePawnSet);

	// 생존 상태의 원본은 URogueHealthComponent
	// bIsAlive는 URogueHealthComponent의 데이터를 파생
	if (HealthComponent)
	{
		HealthComponent->OnDeathStateChanged.AddDynamic(this, &AShooterPlayerState::HandleDeathStateChanged);
	}

	// 구독보다 폰 연결이 먼저였다면 브로드캐스트를 놓친 것이므로 여기서 처리
	if (APawn* ExistingPawn = GetPawn())
	{
		InitializeForAvatar(ExistingPawn);
	}
}

void AShooterPlayerState::HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn)
{
	// Pawn이 바뀌면 ASC를 새 Pawn에 연결
	if (NewPawn)
	{
		InitializeForAvatar(NewPawn);
		OnRep_Appearances();
	}
	else if (OldPawn && AbilitySystemComponent->GetAvatarActor() == OldPawn)
	{
		AbilitySystemComponent->CancelAllAbilities();
		AbilitySystemComponent->ClearAbilityInput();
		if (UCombatComponent* Combat = OldPawn->FindComponentByClass<UCombatComponent>())
		{
			Combat->ResetAiming();
		}
		AbilitySystemComponent->InitAbilityActorInfo(this, nullptr);
	}
}

void AShooterPlayerState::HandleDeathStateChanged(bool bNewIsDead)
{
	SetAlive(!bNewIsDead);
	
	// 사망 시 모든 어빌리티 취소
	if (bNewIsDead && AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
		AbilitySystemComponent->ClearAbilityInput();
		if (AActor* Avatar = AbilitySystemComponent->GetAvatarActor(); IsValid(Avatar))
		{
			if (UCombatComponent* Combat = Avatar->FindComponentByClass<UCombatComponent>())
			{
				Combat->ResetAiming();
			}
		}
	}
}

void AShooterPlayerState::InitializeForAvatar(AActor* Avatar)
{
	if (!ensureMsgf(Avatar, TEXT("[ShooterPlayerState] %s: Avatar 가 없습니다"), *GetName()))
	{
		return;
	}

	const bool bAvatarChanged = AbilitySystemComponent->GetAvatarActor() != Avatar;
	if (bAvatarChanged)
	{
		// 취소는 새 아바타를 연결하기 전에 수행한다. 같은 아바타 재초기화는 조준을 유지한다.
		AbilitySystemComponent->CancelAllAbilities();
		AbilitySystemComponent->ClearAbilityInput();
		if (AActor* PreviousAvatar = AbilitySystemComponent->GetAvatarActor(); IsValid(PreviousAvatar))
		{
			if (UCombatComponent* Combat = PreviousAvatar->FindComponentByClass<UCombatComponent>())
			{
				Combat->ResetAiming();
			}
		}
	}

	// 소유자는 PlayerState, 아바타는 캐릭터.
	AbilitySystemComponent->InitAbilityActorInfo(this, Avatar);
	const APawn* AvatarPawn = Cast<APawn>(Avatar);
	const bool bOwnsAimingState = Avatar->HasAuthority() || Avatar->GetLocalRole() == ROLE_AutonomousProxy
		|| (IsValid(AvatarPawn) && AvatarPawn->IsLocallyControlled());
	if (bAvatarChanged && bOwnsAimingState)
	{
		if (UCombatComponent* Combat = Avatar->FindComponentByClass<UCombatComponent>())
		{
			Combat->ResetAiming();
		}
	}

	// ASC가 Pawn보다 오래 살기 때문에 서버·클라이언트 모두 새 아바타의 이동 상태로 맞춘다.
	if (const AShooterCharacter* ShooterAvatar = Cast<AShooterCharacter>(Avatar))
	{
		const UCharacterMovementComponent* Movement = ShooterAvatar->GetCharacterMovement();
		AbilitySystemComponent->SetLooseGameplayTagCount(RogueTags::State_Movement_Crouching,
			ShooterAvatar->bIsCrouched ? 1 : 0);
		AbilitySystemComponent->SetLooseGameplayTagCount(RogueTags::State_Movement_InAir,
			Movement && Movement->IsFalling() ? 1 : 0);
		AbilitySystemComponent->SetLooseGameplayTagCount(RogueTags::State_Movement_Sprinting, 0);
	}

	HealthComponent->InitializeWithAbilitySystem(AbilitySystemComponent);
	AugmentInventory->InitializeWithAbilitySystem(AbilitySystemComponent);
	WeaponInventory->InitializeWithAbilitySystem(AbilitySystemComponent);

	FGameplayTag InitialWeaponTag;
	if (const UCombatComponent* Combat = Avatar->FindComponentByClass<UCombatComponent>())
	{
		if (const AWeaponBase* EquippedWeapon = Combat->GetEquippedWeapon())
		{
			InitialWeaponTag = EquippedWeapon->GetEquippedStateTag();
		}
	}

	if (InitialWeaponTag.IsValid())
	{
		AbilitySystemComponent->SetEquippedWeaponType(InitialWeaponTag);
	}

	// 초기화는 한번만 수행
	if (HasAuthority() && !bVitalsInitialized)
	{
		bVitalsInitialized = true;
		ServerResetVitals();

		// 베이스의 bIsAlive 기본값인 false에 맞춰 초기화
		SetAlive(!HealthComponent->IsDead());
	}

	if (HasAuthority() && !bStartupAbilitiesGranted)
	{
		bStartupAbilitiesGranted = true;
		ServerGrantStartupAbilities();
	}

	UE_LOG(LogShooterPlayerState, Log,
		TEXT("[ShooterPlayerState] [%s] %s(%p): 아바타 %s(%p) 연결 (%s)"),
		*GetOutermost()->GetName(), *GetName(), this,
		*Avatar->GetName(), Avatar,
		HasAuthority() ? TEXT("서버") : TEXT("클라이언트"));
}

void AShooterPlayerState::ServerRespawnReset()
{
	if (!ensureMsgf(HasAuthority(),
		TEXT("[ShooterPlayerState] %s: 권한이 없는 곳에서 리스폰 초기화를 하려 했습니다"), *GetName()))
	{
		return;
	}

	if (!HealthComponent)
	{
		UE_LOG(LogShooterPlayerState, Error,
			TEXT("[ShooterPlayerState] %s: HealthComponent 가 없어 리스폰 초기화를 완료할 수 없습니다"), *GetName());
		SetAlive(false);
		return;
	}

	// 사망 플래그를 먼저 지운 후 바이탈 초기화
	HealthComponent->ServerResetDeathState();
	ServerResetVitals();
	SetAlive(!HealthComponent->IsDead());
}

void AShooterPlayerState::SetLobbyReady(bool bNewLobbyReady)
{
	if (!HasAuthority() || bIsLobbyReady == bNewLobbyReady)
	{
		return;
	}

	bIsLobbyReady = bNewLobbyReady;
	UE_LOG(LogShooterPlayerState, Log,
		TEXT("[ShooterPlayerState] %s lobby ready changed: %s"),
		*GetNameSafe(this),
		bIsLobbyReady ? TEXT("true") : TEXT("false"));
	OnRep_LobbyReady();
	ForceNetUpdate();

	if (ALobbyGameState* LobbyGameState = GetWorld() ? GetWorld()->GetGameState<ALobbyGameState>() : nullptr)
	{
		LobbyGameState->NotifyLobbyPlayerListChanged();
	}
}

void AShooterPlayerState::OnRep_LobbyReady()
{
	if (ALobbyGameState* LobbyGameState = GetWorld() ? GetWorld()->GetGameState<ALobbyGameState>() : nullptr)
	{
		LobbyGameState->OnLobbyPlayerListChanged.Broadcast();
	}
}

void AShooterPlayerState::OnRep_Team(const EGameTeam PreviousTeam)
{
	Super::OnRep_Team(PreviousTeam);

	AShooterCharacter::RefreshTeamOutlineForAll(GetWorld());
	ARogueDeployableActor::RefreshTeamOutlineForAll(GetWorld());

	// Team 과 LobbyRevision 은 서로 다른 액터에 있어 도착 순서가 보장되지 않음
	// 리비전이 먼저 도착하면 낡은 팀으로 리스트가 그려지므로 여기서 한 번 더 갱신
	if (ALobbyGameState* LobbyGameState = GetWorld() ? GetWorld()->GetGameState<ALobbyGameState>() : nullptr)
	{
		LobbyGameState->OnLobbyPlayerListChanged.Broadcast();
	}
}

void AShooterPlayerState::SetLoadedIntoMatch(bool bNewLoadedIntoMatch)
{
	if (!HasAuthority() || bLoadedIntoMatch == bNewLoadedIntoMatch)
	{
		return;
	}

	bLoadedIntoMatch = bNewLoadedIntoMatch;
	OnRep_LoadedIntoMatch();
	ForceNetUpdate();
}

void AShooterPlayerState::OnRep_LoadedIntoMatch()
{
}

void AShooterPlayerState::SetParticipationState(const EPlayerParticipationState NewParticipationState)
{
	if (!HasAuthority() || ParticipationState == NewParticipationState)
	{
		return;
	}

	ParticipationState = NewParticipationState;
	OnRep_ParticipationState();
	ForceNetUpdate();
}

void AShooterPlayerState::OnRep_ParticipationState()
{
	OnParticipationStateChanged.Broadcast(ParticipationState);
}

void AShooterPlayerState::SetVoiceTalking(bool bNewVoiceTalking)
{
	if (!HasAuthority() || bIsVoiceTalking == bNewVoiceTalking)
	{
		return;
	}

	bIsVoiceTalking = bNewVoiceTalking;
	OnRep_VoiceTalking();
	ForceNetUpdate();
}

void AShooterPlayerState::OnRep_VoiceTalking()
{
	OnVoiceTalkingChanged.Broadcast(this, bIsVoiceTalking);
}

void AShooterPlayerState::ServerAddKill(const int32 Amount)
{
	if (!ensure(HasAuthority()) || Amount == 0)
	{
		return;
	}

	Kills = FMath::Max(0, Kills + Amount);
	OnRep_Kills();
	ForceNetUpdate();
}

void AShooterPlayerState::ServerAddDeath(const int32 Amount)
{
	if (!ensure(HasAuthority()) || Amount == 0)
	{
		return;
	}

	Deaths = FMath::Max(0, Deaths + Amount);
	OnRep_Deaths();
	ForceNetUpdate();
}

int32 AShooterPlayerState::ServerRegisterKillStreak()
{
	if (!HasAuthority())
	{
		return 1;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	CurrentKillStreak = (Now - LastKillTimeSeconds <= MultiKillWindowSeconds) ? CurrentKillStreak + 1 : 1;
	LastKillTimeSeconds = Now;

	return CurrentKillStreak;
}

void AShooterPlayerState::OnRep_Kills()
{
	OnKillStatsChanged.Broadcast(this, Kills, Deaths);
}

void AShooterPlayerState::OnRep_Deaths()
{
	OnKillStatsChanged.Broadcast(this, Kills, Deaths);
}

void AShooterPlayerState::ServerGrantStartupAbilities()
{
	const TSoftObjectPtr<URogueAbilitySet>& SetPtr = GetDefault<URogueGASSettings>()->StartupAbilitySet;

	if (!ensureMsgf(!SetPtr.IsNull(),
		TEXT("[ShooterPlayerState] StartupAbilitySet이 지정되지 않았습니다. "
		     "프로젝트 설정 > 게임 > Rogue GAS > Startup Ability Set을 확인하세요.")))
	{
		return;
	}

	const URogueAbilitySet* AbilitySet = SetPtr.LoadSynchronous();
	if (!ensureMsgf(AbilitySet,
		TEXT("[ShooterPlayerState] StartupAbilitySet 을 로드하지 못했습니다 (%s). "
		     "프로젝트 설정 > 게임 > Rogue GAS 를 확인하세요"), *SetPtr.ToString()))
	{
		return;
	}

	AbilitySystemComponent->GrantAbilitySet(AbilitySet);
}

void AShooterPlayerState::ServerResetVitals()
{
	check(VitalSet);

	// 라운드 한정 GE 제거
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveRoundScopedEffects();
	}

	// 최대치의 베이스만 초기값으로 되돌린다. Infinite GE 증강의 modifier 는 ASC aggregator 에 남는다.
	VitalSet->SetMaxHealth(InitialMaxHealth);
	VitalSet->SetMaxShield(InitialMaxShield);

	// MaxHealth 증강이 유지된 라운드에서는 증가분만큼 현재 체력도 복구한다.
	// InitialHealth < InitialMaxHealth 로 설정한 게임도 같은 시작 결손량을 유지한다.
	const float EffectiveMaxHealth = VitalSet->GetMaxHealth();
	const float HealthBonusFromPersistentEffects = EffectiveMaxHealth - InitialMaxHealth;

	VitalSet->SetHealth(FMath::Clamp(InitialHealth + HealthBonusFromPersistentEffects, 0.f, EffectiveMaxHealth));

	// 보호막은 증강이 부여한 기본 보호막으로 초기화
	VitalSet->SetShield(VitalSet->GetMaxShield());

	VitalSet->SetStamina(VitalSet->GetMaxStamina());

	UE_LOG(LogShooterPlayerState, Log,
		TEXT("[ShooterPlayerState] %s: 바이탈 초기화 체력 %.0f/%.0f 보호막 %.0f/%.0f"),
		*GetName(),
		VitalSet->GetHealth(), VitalSet->GetMaxHealth(),
		VitalSet->GetShield(), VitalSet->GetMaxShield());
}
