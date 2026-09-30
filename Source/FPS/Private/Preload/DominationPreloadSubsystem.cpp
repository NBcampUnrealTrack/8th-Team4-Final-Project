#include "Preload/DominationPreloadSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Loading/LoadingScreenSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationPreload, Log, All);

namespace
{
	bool IsDominationMap(const FString& MapNameOrPath)
	{
		return MapNameOrPath.Contains(TEXT("Domination"));
	}
}

void UDominationPreloadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<ULoadingScreenSubsystem>();

	SeamlessTravelHandle = FWorldDelegates::OnSeamlessTravelStart.AddUObject(
		this, &ThisClass::HandleSeamlessTravelStart);
	SeamlessTransitionHandle = FWorldDelegates::OnSeamlessTravelTransition.AddUObject(
		this, &ThisClass::HandleSeamlessTravelTransition);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this, &ThisClass::HandlePostLoadMapWithWorld);
}

void UDominationPreloadSubsystem::Deinitialize()
{
	FWorldDelegates::OnSeamlessTravelStart.Remove(SeamlessTravelHandle);
	FWorldDelegates::OnSeamlessTravelTransition.Remove(SeamlessTransitionHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	bTravelProgressStarted = false;
	Super::Deinitialize();
}

void UDominationPreloadSubsystem::HandleSeamlessTravelStart(
	UWorld* DepartingWorld, const FString& DestinationMap)
{
	if (!IsOwnedByThisGameInstance(DepartingWorld))
	{
		return;
	}

	bTravelProgressStarted = false;
	if (IsDominationMap(DestinationMap))
	{
		UE_LOG(LogDominationPreload, Log,
			TEXT("[DominationTravel] SeamlessStart T=%.3f From=%s Destination=%s"),
			FPlatformTime::Seconds(), *GetNameSafe(DepartingWorld), *DestinationMap);
		EnsurePreloadStarted(DepartingWorld);
	}
}

void UDominationPreloadSubsystem::HandleSeamlessTravelTransition(UWorld* World)
{
	if (bTravelProgressStarted && IsOwnedByThisGameInstance(World))
	{
		UE_LOG(LogDominationPreload, Log,
			TEXT("[DominationTravel] SeamlessTransition T=%.3f World=%s Elapsed=%.3fs"),
			FPlatformTime::Seconds(), *GetNameSafe(World),
			FPlatformTime::Seconds() - TravelStartSeconds);
	}
}

void UDominationPreloadSubsystem::HandlePostLoadMapWithWorld(UWorld* LoadedWorld)
{
	if (bTravelProgressStarted && IsOwnedByThisGameInstance(LoadedWorld))
	{
		UE_LOG(LogDominationPreload, Log,
			TEXT("[DominationTravel] MapPostLoad T=%.3f World=%s Elapsed=%.3fs"),
			FPlatformTime::Seconds(), *GetNameSafe(LoadedWorld),
			FPlatformTime::Seconds() - TravelStartSeconds);
	}
}

void UDominationPreloadSubsystem::EnsurePreloadStarted(const UWorld* ContextWorld)
{
	if (bTravelProgressStarted || !IsOwnedByThisGameInstance(ContextWorld))
	{
		return;
	}
	bTravelProgressStarted = true;
	TravelStartSeconds = FPlatformTime::Seconds();

	UE_LOG(LogDominationPreload, Log,
		TEXT("[Preload] Start T=%.3f World=%s Assets=0 Policy=DemandLoadedAfterWorld"),
		TravelStartSeconds, *GetNameSafe(ContextWorld));

	// 설정 경로만 기록한다. 클래스 선택 전에 모든 Pawn의 하드 참조를 읽지 않는다.
	for (const FDominationCombatClassDefinition& Definition : GetDefault<UDominationClassSettings>()->CombatClasses)
	{
		UE_LOG(LogDominationPreload, Verbose,
			TEXT("[Preload] Deferred Class=%d Pawn=%s Icon=%s Portrait=%s WeaponArt=%s Voice=%s"),
			static_cast<uint8>(Definition.CombatClass),
			*Definition.PawnClass.ToSoftObjectPath().ToString(),
			*Definition.Icon.ToSoftObjectPath().ToString(),
			*Definition.Portrait.ToSoftObjectPath().ToString(),
			*Definition.WeaponArt.ToSoftObjectPath().ToString(),
			*Definition.SelectionVoice.ToSoftObjectPath().ToString());
	}

	// 클래스는 월드 진입 후 선택한다. 맵 이동 전에 필요한 별도 자산은 없다.
	if (ULoadingScreenSubsystem* LoadingScreenSubsystem = GetLoadingScreenSubsystem())
	{
		LoadingScreenSubsystem->BeginTravelProgress();
		LoadingScreenSubsystem->NotifyPreloadComplete();
	}

	UE_LOG(LogDominationPreload, Log,
		TEXT("[Preload] Complete Elapsed=%.3fs Assets=0; selected combat assets load asynchronously before spawn."),
		FPlatformTime::Seconds() - TravelStartSeconds);
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
