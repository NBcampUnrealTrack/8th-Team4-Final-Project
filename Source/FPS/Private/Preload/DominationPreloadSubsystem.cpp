#include "Preload/DominationPreloadSubsystem.h"

#include "Character/ShooterCharacter.h"
#include "Customization/GameplayAppearanceComponent.h"
#include "Customization/GameplayAppearanceConfig.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameMode/LobbyGameMode.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Loading/LoadingScreenSubsystem.h"
#include "Misc/PackageName.h"
#include "Weapon/RogueWeaponSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationPreload, Log, All);

namespace
{
	FString NormalizeMapPath(const FString& MapNameOrPath)
	{
		FString MapPath = MapNameOrPath;
		int32 OptionsIndex = INDEX_NONE;
		if (MapPath.FindChar(TEXT('?'), OptionsIndex))
		{
			MapPath.LeftInline(OptionsIndex);
		}
		return UWorld::RemovePIEPrefix(FPackageName::ObjectPathToPackageName(MapPath));
	}

	bool IsSameMap(const FString& FirstMap, const FString& SecondMap)
	{
		const FString First = NormalizeMapPath(FirstMap);
		const FString Second = NormalizeMapPath(SecondMap);
		if (First.IsEmpty() || Second.IsEmpty()) return false;
		if (First.StartsWith(TEXT("/")) && Second.StartsWith(TEXT("/"))) return First == Second;
		return FPackageName::GetShortName(First) == FPackageName::GetShortName(Second);
	}

	bool IsCombatMap(const FString& MapNameOrPath)
	{
		const FString MapName = FPackageName::GetShortName(NormalizeMapPath(MapNameOrPath));
		return MapName == TEXT("DominationMap") || MapName == TEXT("Design_DominationMap_WP")
			|| MapName == TEXT("TrainingMap");
	}

	void AddAssetPath(TArray<FSoftObjectPath>& Paths, const FSoftObjectPath& Path)
	{
		if (!Path.IsNull()) Paths.AddUnique(Path);
	}
}

void UDominationPreloadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<ULoadingScreenSubsystem>();
	SeamlessTravelHandle = FWorldDelegates::OnSeamlessTravelStart.AddUObject(
		this, &ThisClass::HandleSeamlessTravelStart);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &ThisClass::HandlePostLoadMapWithWorld);
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
	OnPreloadFinished.Clear();
	Super::Deinitialize();
}

void UDominationPreloadSubsystem::PrepareForMap(const FString& DestinationMap)
{
	const FString Destination = NormalizeMapPath(DestinationMap);
	if (bRequestStarted && IsPreparedMap(Destination))
	{
		if (ULoadingScreenSubsystem* LoadingScreen = GetLoadingScreenSubsystem())
		{
			LoadingScreen->BeginTravelProgress();
			if (bPreloadReady) LoadingScreen->NotifyPreloadComplete();
			else if (bPreloadFailed) LoadingScreen->AbandonPreloadWait();
		}
		return;
	}

	ReleasePreload();
	PreparedDestination = Destination;
	bRequestStarted = true;
	PreloadStartTime = FPlatformTime::Seconds();
	const uint32 Generation = RequestGeneration;
	const UWorld* ContextWorld = GetWorld();
	bIncludeVisuals = !IsRunningDedicatedServer()
		&& (!ContextWorld || ContextWorld->GetNetMode() != NM_DedicatedServer);

	const UDominationClassSettings* ClassSettings = GetDefault<UDominationClassSettings>();
	const URogueWeaponSettings* WeaponSettings = GetDefault<URogueWeaponSettings>();
	if (ULoadingScreenSubsystem* LoadingScreen = GetLoadingScreenSubsystem())
	{
		LoadingScreen->BeginTravelProgress();
	}
	if (Destination.IsEmpty() || !ClassSettings || !ClassSettings->HasValidDefinitions() || !WeaponSettings)
	{
		UE_LOG(LogDominationPreload, Error, TEXT("[Preload] Invalid destination or combat class configuration"));
		FinishPreload(false, Generation);
		return;
	}

	TArray<FSoftObjectPath> AssetPaths;
	for (const FDominationCombatClassDefinition& Definition : ClassSettings->CombatClasses)
	{
		AddAssetPath(AssetPaths, Definition.PawnClass.ToSoftObjectPath());
		const FRoguePrimaryWeaponEntry* WeaponEntry = WeaponSettings->PrimaryWeapons.FindByPredicate(
			[&Definition](const FRoguePrimaryWeaponEntry& Entry)
			{
				return Entry.WeaponTag == Definition.PrimaryWeaponTag;
			});
		if (!WeaponEntry || WeaponEntry->WeaponClass.IsNull())
		{
			UE_LOG(LogDominationPreload, Error, TEXT("[Preload] Missing weapon class for %s"),
				*Definition.PrimaryWeaponTag.ToString());
			FinishPreload(false, Generation);
			return;
		}
		AddAssetPath(AssetPaths, WeaponEntry->WeaponClass.ToSoftObjectPath());
		if (bIncludeVisuals)
		{
			AddAssetPath(AssetPaths, Definition.Icon.ToSoftObjectPath());
			AddAssetPath(AssetPaths, Definition.Portrait.ToSoftObjectPath());
			AddAssetPath(AssetPaths, Definition.WeaponArt.ToSoftObjectPath());
			AddAssetPath(AssetPaths, Definition.SelectionVoice.ToSoftObjectPath());
		}
	}

	UE_LOG(LogDominationPreload, Log, TEXT("[Preload] Start Destination=%s Assets=%d"),
		*PreparedDestination, AssetPaths.Num());
	RequestAssetStage(MoveTemp(AssetPaths), Generation, EPreloadStage::CombatClasses);
}

void UDominationPreloadSubsystem::EnsurePreloadStarted(const UWorld* ContextWorld)
{
	if (!IsOwnedByThisGameInstance(ContextWorld)) return;
	const FString CurrentMap = ContextWorld->GetPackage()->GetName();
	if (bRequestStarted && IsPreparedMap(CurrentMap)) return;
	// 로비의 진행 중 요청을 보호하고 알려진 전투맵 진입만 보완한다.
	if (IsCombatMap(CurrentMap)) PrepareForMap(CurrentMap);
}

void UDominationPreloadSubsystem::RequestAssetStage(
	TArray<FSoftObjectPath> AssetPaths, const uint32 Generation, const EPreloadStage Stage)
{
	if (Generation != RequestGeneration || !bRequestStarted) return;
	AssetPaths.RemoveAll([this](const FSoftObjectPath& Path)
	{
		return RequestedAssetPaths.Contains(Path);
	});
	if (AssetPaths.IsEmpty())
	{
		HandleAssetStageLoaded(Generation, Stage);
		return;
	}
	for (const FSoftObjectPath& Path : AssetPaths)
	{
		if (!Path.IsValid())
		{
			UE_LOG(LogDominationPreload, Error, TEXT("[Preload] Invalid asset path %s"), *Path.ToString());
			FinishPreload(false, Generation);
			return;
		}
		RequestedAssetPaths.AddUnique(Path);
	}

	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		AssetPaths, FStreamableDelegate::CreateWeakLambda(this, [this, Generation, Stage]()
		{
			HandleAssetStageLoaded(Generation, Stage);
		}));
	if (Generation != RequestGeneration) return;
	if (!Handle.IsValid())
	{
		FinishPreload(false, Generation);
		return;
	}
	AssetPreloadHandles.Add(MoveTemp(Handle));
}

void UDominationPreloadSubsystem::HandleAssetStageLoaded(const uint32 Generation, const EPreloadStage Stage)
{
	if (Generation != RequestGeneration || !bRequestStarted || bPreloadReady || bPreloadFailed) return;
	for (const FSoftObjectPath& Path : RequestedAssetPaths)
	{
		if (!IsValid(Path.ResolveObject()))
		{
			UE_LOG(LogDominationPreload, Error, TEXT("[Preload] Failed to load %s"), *Path.ToString());
			FinishPreload(false, Generation);
			return;
		}
	}

	if (Stage == EPreloadStage::CombatClasses && bIncludeVisuals)
	{
		TArray<FSoftObjectPath> AppearancePaths;
		for (const FDominationCombatClassDefinition& Definition : GetDefault<UDominationClassSettings>()->CombatClasses)
		{
			const UClass* PawnClass = Definition.PawnClass.Get();
			const AShooterCharacter* PawnCDO = PawnClass ? PawnClass->GetDefaultObject<AShooterCharacter>() : nullptr;
			if (!PawnCDO)
			{
				FinishPreload(false, Generation);
				return;
			}
			const FName ClassId(*StaticEnum<EDominationCombatClass>()->GetNameStringByValue(
				static_cast<int64>(Definition.CombatClass)));
			if (const UGameplayAppearanceComponent* Appearance = PawnCDO->FindComponentByClass<UGameplayAppearanceComponent>())
			{
				Appearance->AppendPreloadAssetPaths(ClassId, AppearanceConfigPaths);
			}
			// First-person clothing is owner-only and loaded by FirstPersonAppearance
			// when this client actually possesses that class. Retaining every class's
			// arms here also keeps unused visuals alive for the whole combat map.
			// Keep shared pawn/weapon/3P preloads intact for remote players and previews.
		}
		for (const FSoftObjectPath& Path : AppearanceConfigPaths) AppearancePaths.AddUnique(Path);
		RequestAssetStage(MoveTemp(AppearancePaths), Generation, EPreloadStage::AppearanceCatalogs);
		return;
	}

	if (Stage == EPreloadStage::AppearanceCatalogs)
	{
		TArray<FSoftObjectPath> Dependencies;
		for (const FSoftObjectPath& Path : AppearanceConfigPaths)
		{
			const UGameplayAppearanceConfig* Config = Cast<UGameplayAppearanceConfig>(Path.ResolveObject());
			if (!Config)
			{
				FinishPreload(false, Generation);
				return;
			}
			AddAssetPath(Dependencies, Config->StudioConfig.ToSoftObjectPath());
			for (const FGameplayAppearanceBody& Body : Config->Bodies)
			{
				AddAssetPath(Dependencies, Body.FaceMesh.ToSoftObjectPath());
			}
		}
		RequestAssetStage(MoveTemp(Dependencies), Generation, EPreloadStage::AppearanceDependencies);
		return;
	}
	FinishPreload(true, Generation);
}

void UDominationPreloadSubsystem::FinishPreload(const bool bSuccess, const uint32 Generation)
{
	if (Generation != RequestGeneration || !bRequestStarted || bPreloadReady || bPreloadFailed) return;
	bPreloadReady = bSuccess;
	bPreloadFailed = !bSuccess;
	UE_LOG(LogDominationPreload, Log, TEXT("[Preload] Finished Destination=%s Success=%d Assets=%d Elapsed=%.2fs"),
		*PreparedDestination, bSuccess, RequestedAssetPaths.Num(), FPlatformTime::Seconds() - PreloadStartTime);
	if (ULoadingScreenSubsystem* LoadingScreen = GetLoadingScreenSubsystem())
	{
		if (bSuccess) LoadingScreen->NotifyPreloadComplete();
		else LoadingScreen->AbandonPreloadWait();
	}
	OnPreloadFinished.Broadcast(bSuccess);
}

void UDominationPreloadSubsystem::ReleasePreload()
{
	// 이전 목적지의 늦은 완료 콜백은 새 요청에 영향을 주지 않는다.
	++RequestGeneration;
	const bool bWasPending = bRequestStarted && !bPreloadReady && !bPreloadFailed;
	for (const TSharedPtr<FStreamableHandle>& Handle : AssetPreloadHandles)
	{
		if (Handle.IsValid() && !Handle->HasLoadCompleted()) Handle->CancelHandle();
	}
	AssetPreloadHandles.Reset();
	RequestedAssetPaths.Reset();
	AppearanceConfigPaths.Reset();
	PreparedDestination.Reset();
	bRequestStarted = false;
	bPreloadReady = false;
	bPreloadFailed = false;
	if (bWasPending)
	{
		if (ULoadingScreenSubsystem* LoadingScreen = GetLoadingScreenSubsystem()) LoadingScreen->AbandonPreloadWait();
	}
}

void UDominationPreloadSubsystem::HandleSeamlessTravelStart(UWorld* DepartingWorld, const FString& DestinationMap)
{
	if (!IsOwnedByThisGameInstance(DepartingWorld)) return;
	if (IsPreparedMap(DestinationMap)) return;
	const ALobbyGameMode* LobbyGameMode = DepartingWorld->GetAuthGameMode<ALobbyGameMode>();
	if ((LobbyGameMode && IsSameMap(LobbyGameMode->GetSelectedGameMapName(), DestinationMap))
		|| IsCombatMap(DestinationMap))
	{
		PrepareForMap(DestinationMap);
	}
	else
	{
		ReleasePreload();
	}
}

void UDominationPreloadSubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (!IsOwnedByThisGameInstance(LoadedWorld) || PreparedDestination.IsEmpty()
		|| IsPreparedMap(LoadedWorld->GetPackage()->GetName()) || LoadedWorld->IsInSeamlessTravel()) return;
	ReleasePreload();
}

bool UDominationPreloadSubsystem::IsPreparedMap(const FString& MapNameOrPath) const
{
	return IsSameMap(PreparedDestination, MapNameOrPath);
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
