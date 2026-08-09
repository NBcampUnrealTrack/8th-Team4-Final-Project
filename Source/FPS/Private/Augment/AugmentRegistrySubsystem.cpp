// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/AugmentRegistrySubsystem.h"

#include "Augment/AugmentDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAugmentRegistry, Log, All);

void UAugmentRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// AssetManager 가 없으면 PrimaryAssetType 스캔이 불가능
	if (!UAssetManager::IsInitialized())
	{
		UE_LOG(LogAugmentRegistry, Error,
			TEXT("[AugmentRegistry] AssetManager 가 없습니다. 증강 카탈로그를 로드하지 못했습니다"));
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

	DefinitionsById.Reset();
	bReady = false;

	Super::Deinitialize();
}

UAugmentDefinition* UAugmentRegistrySubsystem::FindDefinition(const FPrimaryAssetId& DefinitionId) const
{
	const TObjectPtr<UAugmentDefinition>* Found = DefinitionsById.Find(DefinitionId);
	return Found ? *Found : nullptr;
}

TArray<UAugmentDefinition*> UAugmentRegistrySubsystem::GetAllDefinitions() const
{
	TArray<UAugmentDefinition*> Result;
	Result.Reserve(DefinitionsById.Num());

	for (const TPair<FPrimaryAssetId, TObjectPtr<UAugmentDefinition>>& Pair : DefinitionsById)
	{
		Result.Add(Pair.Value);
	}

	return Result;
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
}

void UAugmentRegistrySubsystem::BuildIndex()
{
	DefinitionsById.Reset();

	UAssetManager& Manager = UAssetManager::Get();

	TArray<FPrimaryAssetId> Ids;
	Manager.GetPrimaryAssetIdList(UAugmentDefinition::AugmentAssetType, Ids);

	if (Ids.IsEmpty())
	{
		// ini 의 PrimaryAssetTypesToScan 등록이 빠졌거나 스캔 경로가 다른 경우
		UE_LOG(LogAugmentRegistry, Error,
			TEXT("[AugmentRegistry] '%s' 타입 애셋이 0건입니다. DefaultGame.ini 의 PrimaryAssetTypesToScan 등록과 스캔 경로를 확인하세요"),
			*UAugmentDefinition::AugmentAssetType.ToString());
		return;
	}

	// 계열별로 하나만 있어야 함
	TMap<FGameplayTag, FPrimaryAssetId> LineOwners;

	for (const FPrimaryAssetId& Id : Ids)
	{
		UAugmentDefinition* Definition = Manager.GetPrimaryAssetObject<UAugmentDefinition>(Id);
		if (!Definition)
		{
			UE_LOG(LogAugmentRegistry, Warning, TEXT("[AugmentRegistry] %s 를 로드하지 못했습니다"), *Id.ToString());
			continue;
		}

		// 증강 계열이 없는 경우
		if (!Definition->LineTag.IsValid())
		{
			UE_LOG(LogAugmentRegistry, Error,
				TEXT("[AugmentRegistry] %s 에 LineTag 가 없어 제외합니다"), *Id.ToString());
			continue;
		}

		if (const FPrimaryAssetId* Owner = LineOwners.Find(Definition->LineTag))
		{
			UE_LOG(LogAugmentRegistry, Error,
				TEXT("[AugmentRegistry] 계열 '%s' 를 %s 와 %s 가 함께 쓰고 있습니다. %s 를 제외합니다"),
				*Definition->LineTag.ToString(), *Owner->ToString(), *Id.ToString(), *Id.ToString());
			continue;
		}

		LineOwners.Add(Definition->LineTag, Id);
		DefinitionsById.Add(Id, Definition);
	}
}
