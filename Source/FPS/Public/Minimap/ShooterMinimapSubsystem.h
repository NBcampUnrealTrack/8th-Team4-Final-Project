#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "ShooterMinimapSubsystem.generated.h"

class ABaseTeamPlayerState;
class AShooterMinimapBounds;
class APawn;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMinimapBoundsChanged, AShooterMinimapBounds* /*NewBounds*/);

/**
 * 미니맵 공용 월드 서브시스템.
 *
 * - 모든 넷모드: 레벨에 배치된 AShooterMinimapBounds를 등록받아 맵 위젯에 넘겨준다.
 *   위젯이 먼저 생기든 액터가 먼저 BeginPlay되든 OnMapBoundsChanged로 한 번은 맞춰진다.
 * - 서버 전용: ShooterMinimapLimits::SpotCheckIntervalSeconds 주기 타이머로 "살아있는 적을
 *   상대 팀 누군가가 보고 있는지"를 판정해 ABaseTeamPlayerState::SetSpottedByEnemy에 반영한다.
 *   클라이언트는 그 값의 OnRep 이벤트만 받아 빨간 점을 켜고 끈다.
 */
UCLASS()
class FPS_API UShooterMinimapSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UShooterMinimapSubsystem* Get(const UObject* WorldContextObject);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	void RegisterMapBounds(AShooterMinimapBounds* Bounds);
	void UnregisterMapBounds(AShooterMinimapBounds* Bounds);

	AShooterMinimapBounds* GetMapBounds() const { return MapBounds.Get(); }

	// 등록된 기준이 없으면 레벨에서 직접 찾아 등록한다. BeginPlay 등록을 놓친 경우(레이트 조인 등) 복구용.
	AShooterMinimapBounds* ResolveMapBounds();

	FOnMinimapBoundsChanged OnMapBoundsChanged;

	// 킬캠 재생용 복제 레벨(DynamicDuplicatedLevels)의 사본인지. 사본은 킬캠마다 월드에 붙었다 떨어지므로 기준으로 쓰지 않는다.
	static bool IsKillcamDuplicate(const AActor* Actor);

private:
	void EvaluateSpotting();

	// 관측자 눈 → 대상(머리/몸통) 중 한 점이라도 시야각·거리 안이고 가리는 게 없으면 true
	bool CanObserverSee(const APawn* Observer, const APawn* Target) const;

	TWeakObjectPtr<AShooterMinimapBounds> MapBounds;

	FTimerHandle SpotCheckTimerHandle;
};
