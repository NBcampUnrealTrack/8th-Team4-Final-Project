// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/PlayerParticipationState.h"
#include "Customization/ReplicatedAppearance.h"

#include "ShooterPlayerState.generated.h"

class AShooterPlayerState;
class UAugmentInventoryComponent;
class UCreditWalletComponent;
class URogueAbilitySystemComponent;
class URogueAugmentAttributeSet;
class URogueHealthComponent;
class URogueVitalAttributeSet;
class UShopComponent;
class UShopPurchaseComponent;
class UWeaponInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShooterVoiceTalkingChangedSignature, AShooterPlayerState*, PlayerState, bool, bIsTalking);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FShooterKillStatsChangedSignature, AShooterPlayerState*, PlayerState, int32, Kills, int32, Deaths);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FShooterParticipationStateChangedSignature,
	EPlayerParticipationState,
	ParticipationState);

UCLASS()
class FPS_API AShooterPlayerState : public ABaseTeamPlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AShooterPlayerState();

    bool StoreValidatedAppearance(FName ClassId, const FCustomizationAppearance& Appearance);
    const FReplicatedAppearance* FindAppearance(FName ClassId) const;
    virtual void CopyProperties(APlayerState* PlayerState) override;
    virtual void OverrideWith(APlayerState* PlayerState) override;

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

	UFUNCTION(BlueprintPure, Category = "Shooter|Shop")
	UShopComponent* GetShopComponent() const { return ShopComponent; }

	UFUNCTION(BlueprintPure, Category = "Shooter|Shop")
	UShopPurchaseComponent* GetShopPurchaseComponent() const { return ShopPurchaseComponent; }

	UFUNCTION(BlueprintPure, Category = "Shooter|Weapon")
	UWeaponInventoryComponent* GetWeaponInventory() const { return WeaponInventory; }

	// 서버의 런타임 장착 결과를 서버 ASC와 소유 클라이언트 ASC에 같은 태그로 반영한다.
	void SyncEquippedWeaponState(FGameplayTag EquippedStateTag);

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

	UFUNCTION(BlueprintPure, Category = "Shooter|Match")
	EPlayerParticipationState GetParticipationState() const { return ParticipationState; }

	void SetParticipationState(EPlayerParticipationState NewParticipationState);

	UPROPERTY(BlueprintAssignable, Category = "Shooter|Match")
	FShooterParticipationStateChangedSignature OnParticipationStateChanged;

	UFUNCTION(BlueprintPure, Category = "Shooter|Voice")
	bool IsVoiceTalking() const { return bIsVoiceTalking; }

	void SetVoiceTalking(bool bNewVoiceTalking);

	UPROPERTY(BlueprintAssignable, Category = "Shooter|Voice")
	FShooterVoiceTalkingChangedSignature OnVoiceTalkingChanged;

	UFUNCTION(BlueprintPure, Category = "Shooter|Stats")
	int32 GetKills() const { return Kills; }

	UFUNCTION(BlueprintPure, Category = "Shooter|Stats")
	int32 GetDeaths() const { return Deaths; }

	// 서버 전용
	void ServerAddKill(int32 Amount = 1);
	void ServerAddDeath(int32 Amount = 1);

	// 서버 전용. 확정된 킬 하나를 등록하고 이번 킬이 몇 연속킬(멀티킬)인지 반환한다.
	// 직전 킬로부터 MultiKillWindowSeconds 이내면 스트릭 유지(+1), 아니면 1로 리셋.
	// ShooterGameMode::HandlePlayerEliminated가 자살/팀킬을 걸러낸 뒤 호출.
	int32 ServerRegisterKillStreak();

	UPROPERTY(BlueprintAssignable, Category = "Shooter|Stats")
	FShooterKillStatsChangedSignature OnKillStatsChanged;

protected:
	virtual void PostInitializeComponents() override;

	// 팀 복제가 로비 리비전보다 늦게 도착해도 플레이어 리스트가 갱신되도록 후처리를 추가
	virtual void OnRep_Team(EGameTeam PreviousTeam) override;

private:

    UPROPERTY(ReplicatedUsing=OnRep_Appearances)
    TArray<FReplicatedAppearance> SavedAppearances;
    UFUNCTION() void OnRep_Appearances();

	UFUNCTION()
	void HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);

	UFUNCTION()
	void HandleDeathStateChanged(bool bNewIsDead);

	UFUNCTION()
	void OnRep_LobbyReady();

	UFUNCTION()
	void OnRep_LoadedIntoMatch();

	UFUNCTION()
	void OnRep_ParticipationState();

	UFUNCTION()
	void OnRep_VoiceTalking();

	UFUNCTION()
	void OnRep_Kills();

	UFUNCTION()
	void OnRep_Deaths();

	UFUNCTION(Client, Reliable)
	void ClientSyncEquippedWeaponState(FGameplayTag EquippedStateTag);

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

	UPROPERTY(VisibleAnywhere, Category = "Shooter|Shop")
	TObjectPtr<UShopComponent> ShopComponent;

	UPROPERTY(VisibleAnywhere, Category = "Shooter|Shop")
	TObjectPtr<UShopPurchaseComponent> ShopPurchaseComponent;

	UPROPERTY(VisibleAnywhere, Category = "Shooter|Weapon")
	TObjectPtr<UWeaponInventoryComponent> WeaponInventory;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyReady, VisibleInstanceOnly, BlueprintReadOnly, Category = "Shooter|Lobby", meta = (AllowPrivateAccess = "true"))
	bool bIsLobbyReady = false;

	UPROPERTY(ReplicatedUsing = OnRep_LoadedIntoMatch, VisibleInstanceOnly, BlueprintReadOnly, Category = "Shooter|Loading", meta = (AllowPrivateAccess = "true"))
	bool bLoadedIntoMatch = false;

	UPROPERTY(ReplicatedUsing = OnRep_ParticipationState, VisibleInstanceOnly, BlueprintReadOnly, Category = "Shooter|Match", meta = (AllowPrivateAccess = "true"))
	EPlayerParticipationState ParticipationState = EPlayerParticipationState::WaitingForMatchStart;

	UPROPERTY(ReplicatedUsing = OnRep_VoiceTalking, VisibleInstanceOnly, BlueprintReadOnly, Category = "Shooter|Voice", meta = (AllowPrivateAccess = "true"))
	bool bIsVoiceTalking = false;

	UPROPERTY(ReplicatedUsing = OnRep_Kills, VisibleInstanceOnly, BlueprintReadOnly, Category = "Shooter|Stats", meta = (AllowPrivateAccess = "true"))
	int32 Kills = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Deaths, VisibleInstanceOnly, BlueprintReadOnly, Category = "Shooter|Stats", meta = (AllowPrivateAccess = "true"))
	int32 Deaths = 0;

	// 멀티킬로 인정되는 연속 킬 사이 최대 간격(초). 복제 불필요 - 서버 전용 판정값.
	UPROPERTY(EditDefaultsOnly, Category = "Shooter|Stats")
	float MultiKillWindowSeconds = 5.f;

	// 서버 전용 - 마지막 킬 시각/현재 스트릭. 복제 안 함.
	float LastKillTimeSeconds = -1000.f;
	int32 CurrentKillStreak = 0;
};
