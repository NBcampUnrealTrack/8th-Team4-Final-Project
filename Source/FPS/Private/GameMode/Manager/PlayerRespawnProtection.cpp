#include "GameMode/Manager/PlayerRespawnProtection.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/GameplayTags.h"

void UPlayerRespawnProtection::Initialize(UWorld* InWorld, const float InDuration)
{
    Shutdown();
    BoundWorld = InWorld;
    ProtectionDuration = FMath::Max(0.0f, InDuration);
}

void UPlayerRespawnProtection::Shutdown()
{
    if (FTimerManager* TimerManager = GetTimerManager())
    {
        TimerManager->ClearAllTimersForObject(this);
    }
    ClearAll();
    BoundWorld.Reset();
}

FTimerManager* UPlayerRespawnProtection::GetTimerManager() const
{
    UWorld* World = BoundWorld.Get();
    return IsValid(World) ? &World->GetTimerManager() : nullptr;
}

void UPlayerRespawnProtection::ClearAll()
{
	for (TPair<TWeakObjectPtr<AShooterPlayerState>, FActiveRespawnProtection>& Entry
		: ActiveRespawnProtections)
	{
		ReleaseProtection(Entry.Value);
	}

	ActiveRespawnProtections.Reset();
}

void UPlayerRespawnProtection::BeginProtection(
	AShooterPlayerState* PlayerState)
{
	FTimerManager* TimerManager = GetTimerManager();
	if (TimerManager == nullptr
		|| !IsValid(PlayerState)
		|| !PlayerState->HasAuthority()
		|| ProtectionDuration <= 0.0f)
	{
		return;
	}

	EndProtection(PlayerState);

	UAbilitySystemComponent* AbilitySystem =
		PlayerState->GetAbilitySystemComponent();
	if (!IsValid(AbilitySystem))
	{
		return;
	}

	FActiveRespawnProtection& Protection =
		ActiveRespawnProtections.Add(PlayerState);
	Protection.AbilitySystem = AbilitySystem;
	Protection.WeaponFiredEventTags.AddTag(RogueTags::Event_Weapon_Fired);
	Protection.WeaponFiredDelegate =
		AbilitySystem->AddGameplayEventTagContainerDelegate(
			Protection.WeaponFiredEventTags,
			FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(
				this,
				&ThisClass::HandleProtectedPlayerWeaponFired,
				TWeakObjectPtr<AShooterPlayerState>(PlayerState)));

	AbilitySystem->AddLooseGameplayTag(
		RogueTags::State_Combat_Invulnerable);
	TimerManager->SetTimer(
		Protection.ExpirationTimer,
		FTimerDelegate::CreateUObject(
			this,
			&ThisClass::HandleProtectionExpired,
			TWeakObjectPtr<AShooterPlayerState>(PlayerState)),
		ProtectionDuration,
		false);
}

void UPlayerRespawnProtection::HandleProtectionExpired(
    const TWeakObjectPtr<AShooterPlayerState> PlayerState)
{
    FActiveRespawnProtection Protection;
    if (ActiveRespawnProtections.RemoveAndCopyValue(PlayerState, Protection))
    {
        ReleaseProtection(Protection);
    }
}

void UPlayerRespawnProtection::HandleProtectedPlayerWeaponFired(
	const FGameplayTag,
	const FGameplayEventData*,
	const TWeakObjectPtr<AShooterPlayerState> PlayerState)
{
	EndProtection(PlayerState.Get());
}

void UPlayerRespawnProtection::EndProtection(
	AShooterPlayerState* PlayerState)
{
	FActiveRespawnProtection Protection;
	if (!ActiveRespawnProtections.RemoveAndCopyValue(
		TWeakObjectPtr<AShooterPlayerState>(PlayerState),
		Protection))
	{
		return;
	}

	ReleaseProtection(Protection);
}

void UPlayerRespawnProtection::ReleaseProtection(
	FActiveRespawnProtection& Protection)
{
	if (FTimerManager* TimerManager = GetTimerManager())
	{
		TimerManager->ClearTimer(Protection.ExpirationTimer);
	}

	if (UAbilitySystemComponent* AbilitySystem = Protection.AbilitySystem.Get())
	{
		AbilitySystem->RemoveGameplayEventTagContainerDelegate(
			Protection.WeaponFiredEventTags,
			Protection.WeaponFiredDelegate);
		AbilitySystem->RemoveLooseGameplayTag(
			RogueTags::State_Combat_Invulnerable);
	}
}

