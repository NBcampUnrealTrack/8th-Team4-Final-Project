#include "Preload/DominationPreloadSubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Loading/LoadingScreenSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationPreload, Log, All);

namespace
{
	// 맵 경로에 이 토큰이 들어가면 Domination 으로 본다.
	const TCHAR* DominationMapToken = TEXT("Domination");

	bool IsDominationMap(const FString& MapNameOrPath)
	{
		return MapNameOrPath.Contains(DominationMapToken);
	}

	const TCHAR* LexNetMode(const ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Standalone:      return TEXT("Standalone");
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_ListenServer:    return TEXT("ListenServer");
		case NM_Client:          return TEXT("Client");
		default:                 return TEXT("Unknown");
		}
	}
}

void UDominationPreloadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 해제 시 로딩 상태를 닫아야 하므로 로딩 서브시스템이 먼저 생기고 나중에 사라져야 한다
	Collection.InitializeDependency<ULoadingScreenSubsystem>();

	SeamlessTravelHandle = FWorldDelegates::OnSeamlessTravelStart.AddUObject(
		this,
		&ThisClass::HandleSeamlessTravelStart);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this,
		&ThisClass::HandlePostLoadMapWithWorld);
}

void UDominationPreloadSubsystem::Deinitialize()
{
	if (SeamlessTravelHandle.IsValid())
	{
		FWorldDelegates::OnSeamlessTravelStart.Remove(SeamlessTravelHandle);
		SeamlessTravelHandle.Reset();
	}

	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	ReleasePreload();

	Super::Deinitialize();
}

void UDominationPreloadSubsystem::HandleSeamlessTravelStart(
	UWorld* DepartingWorld,
	const FString& DestinationMap)
{
	if (!IsOwnedByThisGameInstance(DepartingWorld)
		|| !IsDominationMap(DestinationMap))
	{
		return;
	}

	EnsurePreloadStarted(DepartingWorld);
}

void UDominationPreloadSubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	// 전환 맵에서는 불리지 않고 최종 목적지에서만 불린다
	if (!IsOwnedByThisGameInstance(LoadedWorld)
		|| IsDominationMap(LoadedWorld->GetMapName()))
	{
		return;
	}

	ReleasePreload();
}

void UDominationPreloadSubsystem::EnsurePreloadStarted(const UWorld* ContextWorld)
{
	if (AssetPreloadHandle.IsValid())
	{
		return;
	}

	const UDominationClassSettings* ClassSettings =
		GetDefault<UDominationClassSettings>();
	if (!IsValid(ClassSettings))
	{
		return;
	}

	TArray<FSoftObjectPath> AssetPaths;
	auto AddAssetPath = [&AssetPaths](const FSoftObjectPath& Path)
	{
		if (Path.IsValid())
		{
			AssetPaths.AddUnique(Path);
		}
	};

	for (const FDominationCombatClassDefinition& Definition : ClassSettings->CombatClasses)
	{
		AddAssetPath(Definition.PawnClass.ToSoftObjectPath());
		AddAssetPath(Definition.Icon.ToSoftObjectPath());
		AddAssetPath(Definition.Portrait.ToSoftObjectPath());
		AddAssetPath(Definition.WeaponArt.ToSoftObjectPath());
		AddAssetPath(Definition.SelectionVoice.ToSoftObjectPath());
	}

	if (AssetPaths.IsEmpty())
	{
		return;
	}

	PreloadStartTime = FPlatformTime::Seconds();

	UE_LOG(LogDominationPreload, Log,
		TEXT("[Preload] Start World=%s NetMode=%s Assets=%d"),
		*GetNameSafe(ContextWorld),
		IsValid(ContextWorld) ? LexNetMode(ContextWorld->GetNetMode()) : TEXT("None"),
		AssetPaths.Num());

	// 완료 콜백보다 반드시 먼저 열린다
	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		LoadingScreenSubsystem->BeginTravelProgress();
	}

	AssetPreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		AssetPaths,
		FStreamableDelegate::CreateWeakLambda(this, [this]()
		{
			UE_LOG(LogDominationPreload, Log,
				TEXT("[Preload] Complete Elapsed=%.2fs"),
				FPlatformTime::Seconds() - PreloadStartTime);

			// 실제 완료 90% 를 여는 유일한 경로다
			if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
			{
				LoadingScreenSubsystem->NotifyPreloadComplete();
			}
		}));
}

void UDominationPreloadSubsystem::ReleasePreload()
{
	if (!AssetPreloadHandle.IsValid())
	{
		return;
	}

	UE_LOG(LogDominationPreload, Log,
		TEXT("[Preload] Release Completed=%d"),
		AssetPreloadHandle->HasLoadCompleted());

	// 핸들을 놓으면 완료 콜백도 사라진다. 완료가 아니므로 게이트만 푼다
	if (!AssetPreloadHandle->HasLoadCompleted())
	{
		if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
		{
			LoadingScreenSubsystem->AbandonPreloadWait();
		}
	}

	AssetPreloadHandle.Reset();
}

bool UDominationPreloadSubsystem::IsOwnedByThisGameInstance(const UWorld* World) const
{
	return IsValid(World) && World->GetGameInstance() == GetGameInstance();
}

ULoadingScreenSubsystem* UDominationPreloadSubsystem::GetLoadingScreenSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<ULoadingScreenSubsystem>() : nullptr;
}
