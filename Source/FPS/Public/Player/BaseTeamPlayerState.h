#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/TeamTypes.h"
#include "GameFramework/PlayerState.h"
#include "BaseTeamPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerTeamChangedSignature, EGameTeam, PreviousTeam, EGameTeam, NewTeam);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerAliveStateChangedSignature, bool, bNewIsAlive);

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

	void SetTeam(EGameTeam NewTeam);
	void SetAlive(bool bNewIsAlive);

protected:
	// 파생 클래스(로비 등)가 팀 복제 시점에 후처리를 붙일 수 있도록 virtual 로 변경
	UFUNCTION()
	virtual void OnRep_Team(EGameTeam PreviousTeam);

	UFUNCTION()
	void OnRep_IsAlive();

private:
	UPROPERTY(ReplicatedUsing = OnRep_Team, VisibleInstanceOnly, Category = "Game Flow|Team")
	EGameTeam Team = EGameTeam::None;

	UPROPERTY(ReplicatedUsing = OnRep_IsAlive, VisibleInstanceOnly, Category = "Game Flow|Round")
	bool bIsAlive = false;

public:
	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Team")
	FOnPlayerTeamChangedSignature OnPlayerTeamChanged;

	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Round")
	FOnPlayerAliveStateChangedSignature OnPlayerAliveStateChanged;
};
