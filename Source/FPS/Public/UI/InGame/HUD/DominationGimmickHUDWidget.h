#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Actor/DominationPointGimmick.h"
#include "GameMode/Types/DominationTypes.h"
#include "DominationGimmickHUDWidget.generated.h"

class UWidget;
class UTextBlock;
class UAbilitySystemComponent;
struct FGameplayTag;

/**
 * 활성 거점 기믹(정찰 단말 / 중계 비콘) HUD.
 * - 기믹 위치 마커: 화면 안에 있을 때만 투영 위치를 제공한다(화면 밖 화살표 없음).
 * - 상호작용 안내/진행: 로컬 플레이어가 쓸 수 있으면 [F] 안내, 본인이 누르는 중이면 진행 바.
 * - 팀 알림: 상대 팀 정찰에 우리 팀이 노출되는 중인지와 남은 시간.
 * 계산은 C++에서 하고 모양·배치는 Blueprint에서 한다.
 *
 * 갱신 방식 (피드백 반영):
 * - 마커 위치: 카메라에 따라 매 프레임 바뀌므로 C++ NativeTick에서 MarkerRoot 위젯을 직접 옮긴다.
 * - 상태 표시: 기믹 상태가 바뀌면 즉시 BP_OnGimmickHUDRefresh를 호출한다.
 * - 남은 초·거리: 기믹이 켜진 동안만 RefreshInterval 간격으로 BP_OnGimmickHUDRefresh를 호출한다.
 */
UCLASS(Abstract, Blueprintable)
class FPS_API UDominationGimmickHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 켜진 기믹이 없으면 nullptr
	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick HUD")
	ADominationPointGimmick* GetGimmick() const;

	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick HUD")
	EDominationGimmickType GetGimmickType() const;
	
	// 누군가 상호작용 중이면 0~1, 아니면 0
	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick HUD|Marker")
	float GetInteractionProgress() const;


	// 정찰 단말 재사용 대기 남은 초(올림). 대기 중이 아니면 0
	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick HUD|Marker")
	int32 GetCooldownSecondsRemaining() const;

	// 마커 아이콘 색 판단용: 누르는 팀 → (비콘이면) 소유 팀 → Neutral 순서
	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick HUD|Marker")
	EDominationCaptureRelation GetMarkerRelation() const;

	// 마커 아래 글자: 재사용 대기 중이면 "대기 N초", 아니면 "Nm"
	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick HUD|Marker")
	FText GetMarkerLabel() const;

	// --- 상호작용 안내/진행 ---
	// 로컬 플레이어가 지금 F를 누르면 시작할 수 있는지
	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick HUD|Interaction")
	bool ShouldShowPrompt() const;

	// 로컬 플레이어가 누르고 있는 중인지 (진행 바 표시용)
	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick HUD|Interaction")
	bool IsLocalPlayerInteracting() const;

	// 내 캐릭터가 지금 발각(State.Combat.Revealed) 상태인지. AlertRoot 표시 기준
	UFUNCTION(BlueprintPure, Category = "Domination|Gimmick HUD|Alert")
	bool IsLocalPlayerRevealed() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 표시(글자·색·안내·알림)를 다시 그릴 때 호출된다. 상태 변화 시 즉시 + 기믹이 켜진 동안 주기적으로.
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Domination|Gimmick HUD")
	void BP_OnGimmickHUDRefresh();

	// 켜진 기믹이 바뀔 때만 호출된다(없어지면 nullptr). 아이콘 교체처럼 한 번만 할 일을 여기서 한다.
	// BP_OnGimmickHUDRefresh보다 먼저 호출된다.
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Domination|Gimmick HUD")
	void BP_OnActiveGimmickChanged(ADominationPointGimmick* NewGimmick);

	// 기믹 확보가 완료됐을 때 1회 호출
	// SecuringRelation: 확보한 팀이 내 팀이면 Ally, 상대면 Enemy. bTakeover: 상대가 소유하던 중계 비콘을 빼앗은 경우
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Domination|Gimmick HUD")
	void BP_OnGimmickSecured(
		EDominationGimmickType GimmickType,
		EDominationCaptureRelation SecuringRelation,
		bool bTakeover);

	// 마커 루트. Canvas Panel 자식이어야 하며, 위치와 표시 여부는 C++이 매 프레임 설정한다.
	UPROPERTY(BlueprintReadOnly, Category = "Domination|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MarkerRoot;

	// 확보 알림 배너 루트와 문구
	UPROPERTY(BlueprintReadOnly, Category = "Domination|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SecuredBanner;
	
	UPROPERTY(BlueprintReadOnly, Category = "Domination|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SecuredWhoText;

	// "정찰 단말 확보" / "중계 비콘 확보" / "중계 비콘 탈환"
	UPROPERTY(BlueprintReadOnly, Category = "Domination|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SecuredKindText;

	// "내 위치 노출!" 알림 루트. 표시 여부는 내 캐릭터의 발각 태그에 맞춰 설정
	UPROPERTY(BlueprintReadOnly, Category = "Domination|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> AlertRoot;

private:
	// GetMarkerRelation 내부용 
	// 상호작용 중인 팀의 아군/적 관계. 아무도 없으면 Neutral
	EDominationCaptureRelation GetInteractingRelation() const;
	// 중계 비콘 소유 팀의 아군/적 관계. 소유 팀이 없으면 Neutral
	EDominationCaptureRelation GetOwningRelation() const;

	UFUNCTION()
	void HandleGimmickStateChanged(ADominationPointGimmick* Gimmick);

	UFUNCTION()
	void HandleGimmickSecured(ADominationPointGimmick* Gimmick, EGameTeam SecuringTeam, bool bTakeover);

	void HandleRefreshTimer();
	void RefreshActiveGimmick();
	void RefreshMarkerPlacement();
	void ApplyMarkerToWidget();
	void ApplyAlertToWidget();
	// 로컬 ASC의 발각 태그 변화를 구독한다. ASC가 늦게 생기거나 바뀌어도 다시 구독한다.
	void RefreshRevealedTagBinding();
	void UnbindRevealedTag();
	void HandleRevealedTagChanged(const FGameplayTag Tag, int32 NewCount);
	UAbilitySystemComponent* GetLocalAbilitySystem() const;
	EGameTeam GetLocalTeam() const;

	// 한 프레임에 여러 OnRep이 와도 다음 틱에 한 번만 갱신한다.
	bool bRefreshPending = false;
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;
	FDelegateHandle RevealedTagHandle;

	TArray<TWeakObjectPtr<ADominationPointGimmick>> BoundGimmicks;
	TWeakObjectPtr<ADominationPointGimmick> ActiveGimmick;
	FTimerHandle RefreshTimerHandle;
	bool bShouldShowMarker = false;
	FVector2D MarkerScreenPosition = FVector2D::ZeroVector;
	float DistanceMeters = -1.0f;

	// --- 확보 알림 문구·색 (Class Defaults에서 수정 가능) ---
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Gimmick HUD|Secured")
	FText AllyWhoText = NSLOCTEXT("DominationGimmickHUD", "AllyWho", "아군이 ");

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Gimmick HUD|Secured")
	FText EnemyWhoText = NSLOCTEXT("DominationGimmickHUD", "EnemyWho", "적이 ");

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Gimmick HUD|Secured")
	FText ReconSecuredText = NSLOCTEXT("DominationGimmickHUD", "ReconSecured", "정찰 단말을 확보했습니다");

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Gimmick HUD|Secured")
	FText RelaySecuredText = NSLOCTEXT("DominationGimmickHUD", "RelaySecured", "중계 비콘을 확보했습니다");

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Gimmick HUD|Secured")
	FText RelayTakeoverText = NSLOCTEXT("DominationGimmickHUD", "RelayTakeover", "중계 비콘을 탈환했습니다");

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Gimmick HUD|Secured")
	FLinearColor AllySecuredColor = FLinearColor(0.2f, 0.9f, 0.3f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Gimmick HUD|Secured")
	FLinearColor EnemySecuredColor = FLinearColor(0.95f, 0.2f, 0.2f, 1.0f);

	// 배너가 보이는 시간 (초). 연달아 확보되면 마지막 확보 기준으로 다시 센다.
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Gimmick HUD|Secured", meta = (ClampMin = "0.1", Units = "s"))
	float SecuredBannerDuration = 2.5f;

	FTimerHandle SecuredHideTimerHandle;
	void HideSecuredBanner();

	// 남은 초·거리 갱신 간격 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Gimmick HUD", meta = (ClampMin = "0.05", Units = "s"))
	float RefreshInterval = 0.25f;

	// 마커를 기믹 원점에서 얼마나 위에 띄울지 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Gimmick HUD", meta = (Units = "cm"))
	float MarkerHeightOffset = 150.0f;
};
