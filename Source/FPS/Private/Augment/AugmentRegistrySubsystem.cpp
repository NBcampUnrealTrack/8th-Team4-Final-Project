// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/AugmentRegistrySubsystem.h"

#include "Algo/Sort.h"
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
	LinesByTag.Reset();
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

UAugmentDefinition* UAugmentRegistrySubsystem::FindNextLevelInLine(FGameplayTag LineTag, int32 CurrentLevel) const
{
	const FAugmentLine* Line = LinesByTag.Find(LineTag);
	if (!Line)
	{
		return nullptr;
	}

	for (const TObjectPtr<UAugmentDefinition>& Definition : Line->ByLevel)
	{
		if (Definition->Level == CurrentLevel + 1)
		{
			return Definition;
		}
	}

	// 최고 레벨까지 올린 계열
	return nullptr;
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

	UE_LOG(LogAugmentRegistry, Display, TEXT("[AugmentRegistry] 증강 %d종 / 계열 %d개 로드 완료"),
		DefinitionsById.Num(), LinesByTag.Num());
}

void UAugmentRegistrySubsystem::BuildIndex()
{
	DefinitionsById.Reset();
	LinesByTag.Reset();

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

		DefinitionsById.Add(Id, Definition);
		LinesByTag.FindOrAdd(Definition->LineTag).ByLevel.Add(Definition);
	}

	for (TPair<FGameplayTag, FAugmentLine>& Pair : LinesByTag)
	{
		Algo::Sort(Pair.Value.ByLevel,
			[](const TObjectPtr<UAugmentDefinition>& A, const TObjectPtr<UAugmentDefinition>& B)
			{
				return A->Level < B->Level;
			});

		ValidateLine(Pair.Key, Pair.Value);
	}
}

void UAugmentRegistrySubsystem::ValidateLine(const FGameplayTag& LineTag, const FAugmentLine& Line)
{
	// 시작이 1레벨이 아닌 경우
	if (Line.ByLevel[0]->Level != 1)
	{
		UE_LOG(LogAugmentRegistry, Error,
			TEXT("[AugmentRegistry] 계열 '%s' 에 1레벨이 없습니다 (최저 %d). 이 계열은 상점에 나오지 않습니다"),
			*LineTag.ToString(), Line.ByLevel[0]->Level);
	}

	for (int32 Index = 1; Index < Line.ByLevel.Num(); ++Index)
	{
		const int32 Previous = Line.ByLevel[Index - 1]->Level;
		const int32 Current  = Line.ByLevel[Index]->Level;

		if (Current == Previous)
		{
			// 같은 레벨이 둘 이상인 경우
			UE_LOG(LogAugmentRegistry, Error,
				TEXT("[AugmentRegistry] 계열 '%s' 에 %d레벨이 둘 이상입니다"), *LineTag.ToString(), Current);
		}
		else if (Current != Previous + 1)
		{
			// 다음 레벨이 순차적이지 않은 경우
			UE_LOG(LogAugmentRegistry, Error,
				TEXT("[AugmentRegistry] 계열 '%s' 의 레벨이 끊겼습니다 (%d → %d). %d레벨 이상은 획득할 수 없습니다"),
				*LineTag.ToString(), Previous, Current, Current);
		}
	}
}
