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

	// Axis1D 입력. DefaultMappingContext에서 Mouse Wheel Axis를 연결하면
	// ADS 중 가변 줌 무기의 배율을 한 단계씩 조절한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> AdjustADSZoomAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> PushToTalkAction;

	// 크로스헤어 방향으로 라인트레이스해서 팀원에게 핑을 찍는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> PingAction;

	// 채팅 입력창을 연다(Enter). 입력창이 열린 뒤의 Enter/Tab/Esc는 입력창 위젯이 직접 가로챈다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> OpenChatAction;
	// Common actor interaction. If shared with a GAS entry, unhandled input falls back to that ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> InteractAction;

	// Fire/Reload/무기 슬롯 전환 같은 GAS 어빌리티 입력을 코드에 하드코딩하지 않고 태그 계약으로
	// 연결한다. 1번/2번 무기 전환도 InputTag.Ability.WeaponSlotPrimary/Secondary로 여기에 등록한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Abilities")
	TArray<FShooterAbilityInputAction> AbilityInputActions;

	// 준비 단계에서 상점 화면을 열고 닫는 로컬 UI 입력이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> ToggleShopAction;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> ToggleAugmentSelectAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> ToggleInGameMenuAction;

	// 스코어보드. 누르고 있는 동안 열리므로 Started/Completed 둘 다 바인딩한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Input|Actions")
	TObjectPtr<UInputAction> ShowScoreboardAction;
};
