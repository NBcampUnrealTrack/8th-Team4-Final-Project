#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/DominationTypes.h"
#include "Player/ShooterPlayerState.h"
#include "DominationPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnDominationCombatClassChangedSignature,
	EDominationCombatClass,
	SelectedClass);

UCLASS()
class FPS_API ADominationPlayerState : public AShooterPlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Domination|Class")
	EDominationCombatClass GetCombatClass() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Class")
	bool HasSelectedCombatClass() const;

	bool TryConfirmCombatClass(EDominationCombatClass RequestedClass);

	UPROPERTY(BlueprintAssignable, Category = "Domination|Class")
	FOnDominationCombatClassChangedSignature OnCombatClassChanged;

private:
	UFUNCTION()
	void OnRep_CombatClass();

	UPROPERTY(ReplicatedUsing = OnRep_CombatClass, VisibleInstanceOnly, Category = "Domination|Class")
	EDominationCombatClass CombatClass = EDominationCombatClass::Unselected;
};
