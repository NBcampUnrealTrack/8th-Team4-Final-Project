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
	// 실제 발사 간격은 Weapon::CanFireNow()가 RPM으로 제한한다.
	// GA 내부에서 입력 해제를 기다리는 상호작용/드롭 같은 홀드 동작은 false로 둔다.
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

	// UI controls that must remain available while round gameplay input is locked.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input")
	TObjectPtr<UInputMappingContext> SystemMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input")
	int32 SystemMappingPriority = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> PushToTalkAction;

	// Fire/Reload/무기 슬롯 전환 같은 GAS 어빌리티 입력을 코드에 하드코딩하지 않고 태그 계약으로
	// 연결한다. 1번/2번 무기 전환도 InputTag.Ability.WeaponSlotPrimary/Secondary로 여기에 등록한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Abilities")
	TArray<FShooterAbilityInputAction> AbilityInputActions;

	// 준비 단계에서 상점 화면을 열고 닫는 로컬 UI 입력이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> ToggleShopAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> ToggleInGameMenuAction;

	// 스코어보드. 누르고 있는 동안 열리므로 Started/Completed 둘 다 바인딩한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> ShowScoreboardAction;
};
