// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Player/BaseTeamPlayerState.h"

#include "ShooterPlayerState.generated.h"

class UAugmentInventoryComponent;
class UCreditWalletComponent;
class URogueAbilitySystemComponent;
class URogueAugmentAttributeSet;
class URogueHealthComponent;
class URogueVitalAttributeSet;

UCLASS()
class FPS_API AShooterPlayerState : public ABaseTeamPlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AShooterPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Shooter|GAS")
	URogueAbilitySystemComponent* GetRogueAbilitySystemComponent() const { return AbilitySystemComponent; }

	UFUNCTION(BlueprintPure, Category = "Shooter|GAS")
	URogueHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Shooter|Augment")
	UAugmentInventoryComponent* GetAugmentInventory() const { return AugmentInventory; }

	UFUNCTION(BlueprintPure, Category = "Shooter|Economy")
	UCreditWalletComponent* GetCreditWallet() const { return CreditWallet; }

	// 어트리뷰트는 StatResolver 와 ExecCalc 을 통해 읽는 것이 정상 경로
	// 이 둘은 초기화/디버그용
	URogueVitalAttributeSet* GetVitalSet() const { return VitalSet; }
	URogueAugmentAttributeSet* GetAugmentSet() const { return AugmentSet; }

	// 아바타에 ASC 를 연결
	UFUNCTION(BlueprintCallable, Category = "Shooter|GAS")
	void InitializeForAvatar(AActor* Avatar);

	// 라운드 시작/리스폰 시점에 호출
	// 체력/보호막을 초기화하고 사망 상태를 해제한다.
	UFUNCTION(BlueprintCallable, Category = "Shooter|Vitals")
	void ServerRespawnReset();

	UFUNCTION(BlueprintPure, Category = "Shooter|Lobby")
	bool IsLobbyReady() const { return bIsLobbyReady; }

	void SetLobbyReady(bool bNewLobbyReady);

	UFUNCTION(BlueprintPure, Category = "Shooter|Loading")
	bool IsLoadedIntoMatch() const { return bLoadedIntoMatch; }

	void SetLoadedIntoMatch(bool bNewLoadedIntoMatch);

protected:
	virtual void PostInitializeComponents() override;

	// 팀 복제가 로비 리비전보다 늦게 도착해도 플레이어 리스트가 갱신되도록 후처리를 추가
	virtual void OnRep_Team(EGameTeam PreviousTeam) override;

private:
	UFUNCTION()
	void HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);

	UFUNCTION()
	void HandleDeathStateChanged(bool bNewIsDead);

	UFUNCTION()
	void OnRep_LobbyReady();

	UFUNCTION()
	void OnRep_LoadedIntoMatch();

	void ServerResetVitals();

	void ServerGrantStartupAbilities();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Vitals", meta = (ClampMin = "1.0"))
	float InitialMaxHealth = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Vitals", meta = (ClampMin = "0.0"))
	float InitialHealth = 100.f;

	// 최대 보호막은 해당 라운드의 최대 보호막을 의미함, 0부터 시작
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Vitals", meta = (ClampMin = "0.0"))
	float InitialMaxShield = 0.f;

private:
	// 체력/보호막 초기화는 최초 한 번만 수행
	bool bVitalsInitialized = false;

	// 어빌리티 부여도 최초 한 번만. InitializeForAvatar 는 리스폰마다 다시 돈다
	bool bStartupAbilitiesGranted = false;

private:
	UPROPERTY(VisibleAnywhere, Category = "Shooter|GAS")
	TObjectPtr<URogueAbilitySystemComponent> AbilitySystemComponent;

	// AttributeSet은 ASC가 소유
	UPROPERTY()
	TObjectPtr<URogueVitalAttributeSet> VitalSet;

	UPROPERTY()
	TObjectPtr<URogueAugmentAttributeSet> AugmentSet;

	// 사망 상태 관리
	UPROPERTY(VisibleAnywhere, Category = "Shooter|GAS")
	TObjectPtr<URogueHealthComponent> HealthComponent;

	// 증강 인벤토리
	UPROPERTY(VisibleAnywhere, Category = "Shooter|Augment")
	TObjectPtr<UAugmentInventoryComponent> AugmentInventory;

	UPROPERTY(VisibleAnywhere, Category = "Shooter|Economy")
	TObjectPtr<UCreditWalletComponent> CreditWallet;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyReady, VisibleInstanceOnly, BlueprintReadOnly, Category = "Shooter|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bIsLobbyReady = false;

	UPROPERTY(ReplicatedUsing = OnRep_LoadedIntoMatch, VisibleInstanceOnly, BlueprintReadOnly, Category = "Shooter|Loading", meta = (AllowPrivateAccess = "true"))
	bool bLoadedIntoMatch = false;
};
