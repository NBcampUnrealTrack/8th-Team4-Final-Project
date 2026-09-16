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
	EDominationCombatClass GetPreviewCombatClass() const;
	
	UFUNCTION(BlueprintPure, Category = "Domination|Class")
	bool HasSelectedCombatClass() const;
	UFUNCTION(BlueprintPure, Category = "Domination|Class")
	bool HasPreviewCombatClass() const;
	
	bool TryConfirmCombatClass(EDominationCombatClass RequestedClass);
	// 변경 허용 시점은 GameMode가 검증한다. 최초 선택의 잠금 규칙은 유지한다.
	bool TryChangeCombatClass(EDominationCombatClass RequestedClass);
	void ResetCombatClassSelection();
	// 확정 전 임시 선택
	bool TrySetPreviewCombatClass(EDominationCombatClass RequestedClass);
	
	UPROPERTY(BlueprintAssignable, Category = "Domination|Class")
	FOnDominationCombatClassChangedSignature OnCombatClassChanged;
	UPROPERTY(BlueprintAssignable, Category = "Domination|Class")
	FOnDominationCombatClassChangedSignature OnPreviewCombatClassChanged;

private:
	UFUNCTION()
	void OnRep_CombatClass();
	UFUNCTION()
	void OnRep_PreviewCombatClass();

	UPROPERTY(ReplicatedUsing = OnRep_CombatClass, VisibleInstanceOnly, Category = "Domination|Class")
	EDominationCombatClass CombatClass = EDominationCombatClass::Unselected;
	UPROPERTY(ReplicatedUsing = OnRep_PreviewCombatClass, VisibleInstanceOnly, Category = "Domination|Class")
	EDominationCombatClass PreviewCombatClass = EDominationCombatClass::Unselected;
};
