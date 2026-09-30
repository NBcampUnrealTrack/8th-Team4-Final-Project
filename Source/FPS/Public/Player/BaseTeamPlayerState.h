#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/TeamTypes.h"
#include "GameFramework/PlayerState.h"
#include "BaseTeamPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerTeamChangedSignature, EGameTeam, PreviousTeam, EGameTeam, NewTeam);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerAliveStateChangedSignature, bool, bNewIsAlive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerSpottedChangedSignature, bool, bNewIsSpotted);

UCLASS()
class FPS_API ABaseTeamPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ABaseTeamPlayerState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OverrideWith(APlayerState* PlayerState) override;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Team")
	EGameTeam GetTeam() const;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Round")
	bool IsAlive() const;

	// 상대 팀 누군가의 시야에 들어와 있는지. UShooterMinimapSubsystem(서버)이 주기적으로 갱신하고
	// 상대 팀 클라이언트의 맵 위젯이 OnSpottedByEnemyChanged로 빨간 점을 켜고 끈다.
	UFUNCTION(BlueprintPure, Category = "Game Flow|Minimap")
	bool IsSpottedByEnemy() const;

	void SetTeam(EGameTeam NewTeam);
	void SetAlive(bool bNewIsAlive);
	void SetSpottedByEnemy(bool bNewIsSpotted);

protected:
	// 파생 클래스(로비 등)가 팀 복제 시점에 후처리를 붙일 수 있도록 virtual 로 변경
	UFUNCTION()
	virtual void OnRep_Team(EGameTeam PreviousTeam);

	UFUNCTION()
	void OnRep_IsAlive();

	UFUNCTION()
	void OnRep_SpottedByEnemy();

private:
	UPROPERTY(ReplicatedUsing = OnRep_Team, VisibleInstanceOnly, Category = "Game Flow|Team")
	EGameTeam Team = EGameTeam::None;

	UPROPERTY(ReplicatedUsing = OnRep_IsAlive, VisibleInstanceOnly, Category = "Game Flow|Round")
	bool bIsAlive = false;

	UPROPERTY(ReplicatedUsing = OnRep_SpottedByEnemy, VisibleInstanceOnly, Category = "Game Flow|Minimap")
	bool bSpottedByEnemy = false;

public:
	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Team")
	FOnPlayerTeamChangedSignature OnPlayerTeamChanged;

	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Round")
	FOnPlayerAliveStateChangedSignature OnPlayerAliveStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Minimap")
	FOnPlayerSpottedChangedSignature OnSpottedByEnemyChanged;
};
