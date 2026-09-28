// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/AugmentRegistrySubsystem.h"

#include "Augment/AugmentDefinition.h"
#include "Augment/Actor/RogueAugmentActor.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GAS/RogueGASSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogAugmentRegistry, Log, All);

namespace
{
	void AddPreloadPath(TArray<FSoftObjectPath>& OutPaths, const FSoftObjectPath& Path)
	{
		if (!Path.IsNull())
		{
			OutPaths.AddUnique(Path);
		}
	}
}

void UAugmentRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// AssetManager 가 없으면 PrimaryAssetType 스캔이 불가능
	if (!ensureMsgf(UAssetManager::IsInitialized(),
		TEXT("[AugmentRegistry] AssetManager 가 없습니다. 증강 카탈로그를 로드하지 못했습니다")))
	{
		return;
	}

	StartAsyncLoad();
}

void UAugmentRegistrySubsystem::Deinitialize()
{
	if (LoadHandle.IsValid())
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}

	if (PreloadHandle.IsValid())
	{
		PreloadHandle->CancelHandle();
		PreloadHandle.Reset();
	}

	if (FollowUpPreloadHandle.IsValid())
	{
		FollowUpPreloadHandle->CancelHandle();
		FollowUpPreloadHandle.Reset();
	}

	DefinitionsById.Reset();
	AllDefinitions.Reset();
	DefinitionsByTier.Reset();
	ActiveDefinitions.Reset();
	bReady = false;

	Super::Deinitialize();
}

UAugmentDefinition* UAugmentRegistrySubsystem::FindDefinition(const FPrimaryAssetId& DefinitionId) const
{
	const TObjectPtr<UAugmentDefinition>* Found = DefinitionsById.Find(DefinitionId);
	return Found ? *Found : nullptr;
}

const TArray<UAugmentDefinition*>& UAugmentRegistrySubsystem::GetDefinitionsByTier(const EAugmentTier Tier) const
{
	static const TArray<UAugmentDefinition*> Empty;

	const TArray<UAugmentDefinition*>* Found = DefinitionsByTier.Find(Tier);
	return Found ? *Found : Empty;
}

void UAugmentRegistrySubsystem::StartAsyncLoad()
{
	const TArray<FName> MetadataOnly;

	LoadHandle = UAssetManager::Get().LoadPrimaryAssetsWithType(
		UAugmentDefinition::AugmentAssetType,
		MetadataOnly,
		FStreamableDelegate::CreateUObject(this, &UAugmentRegistrySubsystem::OnCatalogLoaded));

	// 로드할 것이 없으면 (스캔 0건 / 이미 전부 메모리에 있음) 델리게이트가 오지 않음
	if (!LoadHandle.IsValid())
	{
		OnCatalogLoaded();
	}
}

void UAugmentRegistrySubsystem::OnCatalogLoaded()
{
	BuildIndex();

	bReady = true;
	OnRegistryReady.Broadcast();

	UE_LOG(LogAugmentRegistry, Display, TEXT("[AugmentRegistry] 증강 계열 %d개 로드 완료"),
		DefinitionsById.Num());

	StartAssetPreload();
}

void UAugmentRegistrySubsystem::BuildIndex()
{
	DefinitionsById.Reset();
	AllDefinitions.Reset();
	DefinitionsByTier.Reset();
	ActiveDefinitions.Reset();

	UAssetManager& Manager = UAssetManager::Get();

	TArray<FPrimaryAssetId> Ids;
	Manager.GetPrimaryAssetIdList(UAugmentDefinition::AugmentAssetType, Ids);

	// ini 의 PrimaryAssetTypesToScan 등록이 빠졌거나 스캔 경로가 다른 경우
	if (!ensureMsgf(!Ids.IsEmpty(),
		TEXT("[AugmentRegistry] '%s' 타입 애셋이 0건입니다. DefaultGame.ini 의 PrimaryAssetTypesToScan 등록과 스캔 경로를 확인하세요"),
		*UAugmentDefinition::AugmentAssetType.ToString()))
	{
		return;
	}

	// 계열별로 하나만 있어야 함
	TMap<FGameplayTag, FPrimaryAssetId> LineOwners;

	for (const FPrimaryAssetId& Id : Ids)
	{
		UAugmentDefinition* Definition = Manager.GetPrimaryAssetObject<UAugmentDefinition>(Id);
		if (!ensureMsgf(Definition, TEXT("[AugmentRegistry] %s 를 로드하지 못했습니다"), *Id.ToString()))
		{
			continue;
		}

		// 증강 계열이 없는 경우
		if (!ensureMsgf(Definition->LineTag.IsValid(),
			TEXT("[AugmentRegistry] %s 에 LineTag 가 없어 제외합니다"), *Id.ToString()))
		{
			continue;
		}

		const FPrimaryAssetId* Owner = LineOwners.Find(Definition->LineTag);
		if (!ensureMsgf(!Owner,
			TEXT("[AugmentRegistry] 계열 '%s' 를 %s 와 %s 가 함께 쓰고 있습니다. %s 를 제외합니다"),
			*Definition->LineTag.ToString(), *Owner->ToString(), *Id.ToString(), *Id.ToString()))
		{
			continue;
		}

		LineOwners.Add(Definition->LineTag, Id);
		DefinitionsById.Add(Id, Definition);
		AllDefinitions.Add(Definition);
		DefinitionsByTier.FindOrAdd(Definition->Tier).Add(Definition);

		if (Definition->AugmentCategory == EAugmentCategory::Active)
		{
			ActiveDefinitions.Add(Definition);
		}
	}
}

// 증강이 쓰는 애셋을 미리 로드해 게임 중 동기 로드를 없앰
void UAugmentRegistrySubsystem::StartAssetPreload()
{
	TArray<FSoftObjectPath> Paths;

	for (const UAugmentDefinition* Definition : AllDefinitions)
	{
		for (const TSoftClassPtr<UGameplayEffect>& Effect : Definition->GrantedEffects)
		{
			AddPreloadPath(Paths, Effect.ToSoftObjectPath());
		}

		for (const FAugmentGrantedAbility& Granted : Definition->GrantedAbilities)
		{
			AddPreloadPath(Paths, Granted.AbilityClass.ToSoftObjectPath());
			AddPreloadPath(Paths, Granted.SpawnActorClass.ToSoftObjectPath());

			for (const TSoftClassPtr<UGameplayEffect>& Effect : Granted.EffectsToApply)
			{
				AddPreloadPath(Paths, Effect.ToSoftObjectPath());
			}
		}

		// 손에 드는 메시는 화면용이라 데디케이티드 서버에선 생략
		if (!IsRunningDedicatedServer())
		{
			AddPreloadPath(Paths, Definition->HeldVisual.Mesh.ToSoftObjectPath());
		}
	}

	const URogueGASSettings* Settings = GetDefault<URogueGASSettings>();
	AddPreloadPath(Paths, Settings->CooldownEffectClass.ToSoftObjectPath());
	AddPreloadPath(Paths, Settings->StatusEffectClass.ToSoftObjectPath());

	if (Paths.IsEmpty())
	{
		return;
	}

	UE_LOG(LogAugmentRegistry, Display, TEXT("[AugmentRegistry] 증강 애셋 %d개 프리로드 시작"), Paths.Num());

	PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Paths, FStreamableDelegate::CreateUObject(this, &ThisClass::OnAssetPreloaded));
}

void UAugmentRegistrySubsystem::OnAssetPreloaded()
{
	TArray<FSoftObjectPath> FollowUps;

	for (const UAugmentDefinition* Definition : AllDefinitions)
	{
		for (const FAugmentGrantedAbility& Granted : Definition->GrantedAbilities)
		{
			if (UClass* ActorClass = Granted.SpawnActorClass.Get())
			{
				const ARogueAugmentActor* ActorCDO = GetDefault<ARogueAugmentActor>(ActorClass);
				AddPreloadPath(FollowUps, ActorCDO->GetSpawnOnDetonateClass().ToSoftObjectPath());
			}
		}
	}

	if (!FollowUps.IsEmpty())
	{
		FollowUpPreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(FollowUps);
	}

	UE_LOG(LogAugmentRegistry, Display, TEXT("[AugmentRegistry] 증강 애셋 프리로드 완료 (후속 액터 %d개)"),
		FollowUps.Num());
}
