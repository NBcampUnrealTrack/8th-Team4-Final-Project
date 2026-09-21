#include "Preload/DominationPreloadSubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameMode/Settings/DominationClassSettings.h"

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
	if (PawnPreloadHandle.IsValid())
	{
		return;
	}

	const UDominationClassSettings* ClassSettings =
		GetDefault<UDominationClassSettings>();
	if (!IsValid(ClassSettings))
	{
		return;
	}

	TArray<FSoftObjectPath> PawnPaths;
	for (const FDominationCombatClassDefinition& Definition : ClassSettings->CombatClasses)
	{
		if (!Definition.PawnClass.IsNull())
		{
			PawnPaths.AddUnique(Definition.PawnClass.ToSoftObjectPath());
		}
	}

	if (PawnPaths.IsEmpty())
	{
		return;
	}

	PreloadStartTime = FPlatformTime::Seconds();

	UE_LOG(LogDominationPreload, Log,
		TEXT("[Preload] Start World=%s NetMode=%s Assets=%d"),
		*GetNameSafe(ContextWorld),
		IsValid(ContextWorld) ? LexNetMode(ContextWorld->GetNetMode()) : TEXT("None"),
		PawnPaths.Num());

	PawnPreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		PawnPaths,
		FStreamableDelegate::CreateWeakLambda(this, [this]()
		{
			UE_LOG(LogDominationPreload, Log,
				TEXT("[Preload] Complete Elapsed=%.2fs"),
				FPlatformTime::Seconds() - PreloadStartTime);
		}));
}

void UDominationPreloadSubsystem::ReleasePreload()
{
	if (!PawnPreloadHandle.IsValid())
	{
		return;
	}

	UE_LOG(LogDominationPreload, Log,
		TEXT("[Preload] Release Completed=%d"),
		PawnPreloadHandle->HasLoadCompleted());

	PawnPreloadHandle.Reset();
}

bool UDominationPreloadSubsystem::IsOwnedByThisGameInstance(const UWorld* World) const
{
	return IsValid(World) && World->GetGameInstance() == GetGameInstance();
}
