#include "GameMode/Manager/DominationLoadoutManager.h"

#include "Character/ShooterCharacter.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GAS/RogueAbilitySet.h"
#include "GAS/RogueGASSettings.h"
#include "GameMode/Types/DominationTypes.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "GameFramework/Pawn.h"
#include "Player/DominationPlayerState.h"
#include "Weapon/RogueWeaponSettings.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/WeaponInventoryComponent.h"
#include "Weapon/WeaponLoadoutService.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationLoadout, Log, All);

namespace
{
	const FRoguePrimaryWeaponEntry* FindPrimaryWeaponEntry(const FGameplayTag WeaponTag)
	{
		return GetDefault<URogueWeaponSettings>()->PrimaryWeapons.FindByPredicate(
			[WeaponTag](const FRoguePrimaryWeaponEntry& Entry)
			{
				return Entry.WeaponTag == WeaponTag;
			});
	}
}

bool UDominationLoadoutManager::HasRequiredWeaponRegistryEntries() const
{
	const UDominationClassSettings* ClassSettings =
		GetDefault<UDominationClassSettings>();
	const URogueWeaponSettings* WeaponSettings = GetDefault<URogueWeaponSettings>();
	if (!IsValid(ClassSettings)
		|| !IsValid(WeaponSettings)
		|| ClassSettings->CombatClasses.IsEmpty())
	{
		return false;
	}

	for (const FDominationCombatClassDefinition& Definition : ClassSettings->CombatClasses)
	{
		const FRoguePrimaryWeaponEntry* Entry = FindPrimaryWeaponEntry(Definition.PrimaryWeaponTag);
		if (Definition.CombatClass == EDominationCombatClass::Unselected
			|| !Definition.PrimaryWeaponTag.IsValid()
			|| !Entry || Entry->WeaponClass.IsNull())
		{
			return false;
		}
	}

	return true;
}

EDominationCombatAssetStatus UDominationLoadoutManager::EnsureCombatAssets(
	const EDominationCombatClass CombatClass)
{
	if (const EDominationCombatAssetStatus* Status = CombatAssetStates.Find(CombatClass))
	{
		return *Status;
	}
	if (AreCombatAssetsReady(CombatClass))
	{
		return EDominationCombatAssetStatus::Ready;
	}

	TArray<FSoftObjectPath> Paths;
	if (!GatherCombatAssetPaths(CombatClass, Paths))
	{
		CombatAssetStates.Add(CombatClass, EDominationCombatAssetStatus::Failed);
		UE_LOG(LogDominationLoadout, Error,
			TEXT("Domination class %d is missing a Pawn, primary weapon, or startup ability set reference."),
			static_cast<uint8>(CombatClass));
		return EDominationCombatAssetStatus::Failed;
	}

	CombatAssetStates.Add(CombatClass, EDominationCombatAssetStatus::Loading);
	CombatAssetRequestTimes.Add(CombatClass, FPlatformTime::Seconds());
	UE_LOG(LogDominationLoadout, Log,
		TEXT("[DominationTravel] Selected combat assets requested: Class=%d Assets=%d"),
		static_cast<uint8>(CombatClass), Paths.Num());
	for (const FSoftObjectPath& Path : Paths)
	{
		UE_LOG(LogDominationLoadout, Log, TEXT("[DominationTravel] Combat asset root: %s"), *Path.ToString());
	}
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Paths,
		FStreamableDelegate::CreateUObject(this, &ThisClass::HandleCombatAssetsLoaded, CombatClass));
	if (!Handle.IsValid())
	{
		CombatAssetStates.Add(CombatClass, EDominationCombatAssetStatus::Failed);
		return EDominationCombatAssetStatus::Failed;
	}
	CombatAssetHandles.Add(CombatClass, MoveTemp(Handle));
	return CombatAssetStates.FindChecked(CombatClass);
}

bool UDominationLoadoutManager::GatherCombatAssetPaths(
	const EDominationCombatClass CombatClass, TArray<FSoftObjectPath>& OutPaths)
{
	const FDominationCombatClassDefinition* Definition =
		GetDefault<UDominationClassSettings>()->FindDefinition(CombatClass);
	const FRoguePrimaryWeaponEntry* Entry = Definition
		? FindPrimaryWeaponEntry(Definition->PrimaryWeaponTag) : nullptr;
	const TSoftObjectPtr<URogueAbilitySet>& StartupAbilitySet = GetDefault<URogueGASSettings>()->StartupAbilitySet;
	if (!Definition || Definition->PawnClass.IsNull() || !Entry || Entry->WeaponClass.IsNull()
		|| StartupAbilitySet.IsNull())
	{
		return false;
	}
	// 공용 보조무기는 선택된 Pawn CDO의 참조에 포함된다.
	OutPaths.AddUnique(Definition->PawnClass.ToSoftObjectPath());
	OutPaths.AddUnique(Entry->WeaponClass.ToSoftObjectPath());
	OutPaths.AddUnique(StartupAbilitySet.ToSoftObjectPath());
	return true;
}

bool UDominationLoadoutManager::AreCombatAssetsReady(const EDominationCombatClass CombatClass)
{
	const FDominationCombatClassDefinition* Definition =
		GetDefault<UDominationClassSettings>()->FindDefinition(CombatClass);
	const FRoguePrimaryWeaponEntry* Entry = Definition
		? FindPrimaryWeaponEntry(Definition->PrimaryWeaponTag) : nullptr;
	return Definition && Definition->PawnClass.Get() && Entry && Entry->WeaponClass.Get()
		&& GetDefault<URogueGASSettings>()->StartupAbilitySet.Get();
}

void UDominationLoadoutManager::HandleCombatAssetsLoaded(const EDominationCombatClass CombatClass)
{
	const bool bReady = AreCombatAssetsReady(CombatClass);
	CombatAssetStates.Add(CombatClass, bReady
		? EDominationCombatAssetStatus::Ready : EDominationCombatAssetStatus::Failed);
	const double* RequestTime = CombatAssetRequestTimes.Find(CombatClass);
	const double Elapsed = RequestTime ? FPlatformTime::Seconds() - *RequestTime : 0.0;
	CombatAssetRequestTimes.Remove(CombatClass);
	UE_LOG(LogDominationLoadout, Log,
		TEXT("[DominationTravel] Selected combat assets completed: Class=%d Success=%d Elapsed=%.3fs"),
		static_cast<uint8>(CombatClass), bReady, Elapsed);
	OnCombatAssetsReady.Broadcast(CombatClass, bReady);
}

void UDominationLoadoutManager::Shutdown()
{
	OnCombatAssetsReady.Clear();
	for (const TPair<EDominationCombatClass, TSharedPtr<FStreamableHandle>>& Entry : CombatAssetHandles)
	{
		if (Entry.Value.IsValid())
		{
			Entry.Value->CancelHandle();
		}
	}
	CombatAssetHandles.Reset();
	CombatAssetStates.Reset();
	CombatAssetRequestTimes.Reset();
}

bool UDominationLoadoutManager::InitializeCombatClassInventory(
	ADominationPlayerState* PlayerState) const
{
	UWeaponInventoryComponent* WeaponInventory = IsValid(PlayerState)
		? PlayerState->GetWeaponInventory()
		: nullptr;
	const FGameplayTag PrimaryWeaponTag = IsValid(PlayerState)
		? ResolvePrimaryWeaponTag(PlayerState->GetCombatClass())
		: FGameplayTag();
	if (!IsValid(WeaponInventory) || !PrimaryWeaponTag.IsValid())
	{
		return false;
	}

	WeaponInventory->ServerInitializeForNewMatch();
	return EnsureCombatClassInventory(PlayerState);
}

bool UDominationLoadoutManager::EnsureCombatClassInventory(
	ADominationPlayerState* PlayerState) const
{
	UWeaponInventoryComponent* WeaponInventory = IsValid(PlayerState)
		? PlayerState->GetWeaponInventory()
		: nullptr;
	const FGameplayTag PrimaryWeaponTag = IsValid(PlayerState)
		? ResolvePrimaryWeaponTag(PlayerState->GetCombatClass())
		: FGameplayTag();
	if (!IsValid(WeaponInventory) || !PrimaryWeaponTag.IsValid())
	{
		return false;
	}

	if (WeaponInventory->GetEquippedPrimaryTag() == PrimaryWeaponTag
		&& WeaponInventory->IsPossessed(PrimaryWeaponTag))
	{
		return true;
	}

	// 레이트 조인/재스폰 중 상태 동기화가 늦었더라도 다른 무기 상태를 리셋하지 않고
	// 해당 클래스의 주무기만 다시 확정한다.
	return WeaponInventory->ServerConfirmPrimaryPurchase(PrimaryWeaponTag);
}

bool UDominationLoadoutManager::ApplyCombatClassLoadout(
	const ADominationPlayerState* PlayerState,
	APawn* Pawn) const
{
	const UWeaponInventoryComponent* WeaponInventory = IsValid(PlayerState)
		? PlayerState->GetWeaponInventory()
		: nullptr;
	if (!IsValid(WeaponInventory) || !IsValid(Pawn))
	{
		return false;
	}

	return RogueWeaponLoadout::ApplyPrimaryWeapon(
		Pawn,
		WeaponInventory->GetEquippedPrimaryTag());
}

FGameplayTag UDominationLoadoutManager::ResolvePrimaryWeaponTag(
	const EDominationCombatClass CombatClass)
{
	const UDominationClassSettings* ClassSettings =
		GetDefault<UDominationClassSettings>();
	const FDominationCombatClassDefinition* Definition = IsValid(ClassSettings)
		? ClassSettings->FindDefinition(CombatClass)
		: nullptr;

	return Definition ? Definition->PrimaryWeaponTag : FGameplayTag();
}
