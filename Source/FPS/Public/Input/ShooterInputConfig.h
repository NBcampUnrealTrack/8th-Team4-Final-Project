#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ShooterInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

USTRUCT(BlueprintType)
struct FShooterAbilityInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input")
	TObjectPtr<UInputAction> InputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input", meta = (Categories = "InputTag.Ability"))
	FGameplayTag InputTag;

	// AR 연사처럼 누르고 있는 동안 매 프레임 활성화를 재시도해야 하는 입력에 사용한다.
	// 실제 발사 간격은 Weapon::CanFireNow()가 RPM으로 제한한다. Q/E/Reload는 false가 기본이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input")
	bool bTriggerWhileHeld = false;
};

UCLASS(BlueprintType)
class FPS_API UShooterInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UShooterInputConfig();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input")
	int32 MappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> SprintAction;

	// Fire/Reload/무기 슬롯 전환 같은 GAS 어빌리티 입력을 코드에 하드코딩하지 않고 태그 계약으로
	// 연결한다. 1번/2번 무기 전환도 InputTag.Ability.WeaponSlotPrimary/Secondary로 여기에 등록한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Abilities")
	TArray<FShooterAbilityInputAction> AbilityInputActions;
};
