// ShooterMapWidget.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameMode/Types/TeamTypes.h"
#include "Minimap/MinimapTypes.h"
#include "Ping/PingTypes.h"
#include "Styling/SlateBrush.h"
#include "TimerManager.h"
#include "ShooterMapWidget.generated.h"

class ABaseTeamPlayerState;
class AShooterMinimapBounds;
class AShooterPlayerController;
class UCanvasPanel;
class UImage;
class UTexture2D;

USTRUCT()
struct FShooterMapPlayerMarker
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UImage> Image;

	TWeakObjectPtr<ABaseTeamPlayerState> Target;

	// true면 바라보는 방향으로 아이콘을 돌린다(화살표). false면 점이라 회전 무의미.
	bool bShowsFacing = false;

	bool bIsLocalPlayer = false;

	// 미니맵 원 밖이면 테두리에 방향으로 붙일지
	bool bClampToEdge = false;
};

USTRUCT()
struct FShooterMapPingMarker
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UImage> Image;

	// 핑은 고정 좌표라 받을 때 한 번만 계산해둔다.
	FVector2D ContentPosition = FVector2D::ZeroVector;

	FTimerHandle ExpiryTimerHandle;
	bool bActive = false;
};

/**
 * 미니맵(좌상단 원형) / 전체 맵(M) 공용 위젯. MapMode로 두 용도를 나눈다.
 *
 * 이벤트 기반으로 "무엇을 그릴지"를 정하고, 타이머로 "어디에 그릴지"만 옮긴다.
 * - 마커 목록 재구성(이벤트): 생존 변경, 팀 변경, 적 발견/해제(OnSpottedByEnemyChanged),
 *   맵 열림, 맵 기준 액터 등록, 인원 수 변화.
 * - 핑(이벤트): OnPingReceived로 생성, 수명 타이머로 제거. 고정 좌표라 이동 갱신 없음.
 * - 위치 이동(타이머): 움직임 자체는 이벤트가 없어서 PositionUpdateInterval 주기로만 샘플링한다.
 *   NativeTick은 쓰지 않고, 전체 맵은 열려 있는 동안에만 타이머가 돈다.
 *
 * 필요한 위젯 계층 (WBP에서 이름을 맞춘다):
 *   Panel_MapViewport (CanvasPanel)      ← 보이는 창. 미니맵은 원형 마스크 RetainerBox로 감싼다.
 *     └ Panel_MapContent (CanvasPanel)   ← 맵 전체. 크기/앵커는 코드가 MapContentSize로 맞춘다.
 *         └ Image_MapBackground (Image)  ← 선으로 그린 배경. Content를 꽉 채우도록 코드가 맞춘다.
 * 마커(UImage)는 코드가 Panel_MapContent에 직접 추가하므로 WBP에 둘 필요 없다.
 *
 * DisableNativeTick: 부모 C++에 이 표시가 없으면 WBP는 NativeTick을 안 써도 엔진이 매 프레임 틱을 돌린다.
 * 이 위젯은 타이머로만 갱신하므로 꺼둔다. (WBP에서 Event Tick이나 애니메이션을 쓰면 그때만 다시 틱이 켜진다.)
 */
UCLASS(meta = (DisableNativeTick))
class FPS_API UShooterMapWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UShooterMapWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Shooter|Map")
	bool IsFullMapOpen() const { return bFullMapOpen; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ---- 이벤트 핸들러 ----
	void HandleMapBoundsChanged(AShooterMinimapBounds* NewBounds);
	void HandleFullMapOpenChanged(bool bOpen);
	void HandleFullMapZoomInput(float WheelDelta);

	UFUNCTION()
	void HandlePingReceived(const FPingEvent& Ping);

	UFUNCTION()
	void HandlePlayerTeamChanged(EGameTeam PreviousTeam, EGameTeam NewTeam);

	UFUNCTION()
	void HandlePlayerAliveChanged(bool bNewIsAlive);

	UFUNCTION()
	void HandlePlayerSpottedChanged(bool bNewIsSpotted);

	void HandlePingExpired(int32 PingIndex);

	// ---- 상태 ----
	void SetFullMapOpen(bool bOpen);
	void StartPositionUpdates();
	void StopPositionUpdates();

	// 마커 목록을 다시 정하고, 위치 갱신이 돌고 있으면 즉시 한 번 반영한다.
	void RefreshMarkers();

	// PlayerArray를 훑어 누구를 어떤 색으로 그릴지 정한다. 위치는 건드리지 않는다.
	void RebuildPlayerMarkers();

	// 타이머 콜백. Content 이동/확대 + 마커 위치만 갱신한다.
	void UpdatePositions();

	void BindPlayerState(ABaseTeamPlayerState* PlayerState);
	void UnbindAllPlayerStates();
	void ClearAllPings();

	void ApplyLayoutToSlots();
	void ApplyBackgroundTexture();

	// 미니맵 중심. 카메라 기준이라 사망 후 관전/킬캠 중에도 보는 곳을 따라간다.
	bool GetViewPoint(FVector& OutLocation, float& OutYaw) const;

	// 미니맵 원 밖이면 원 테두리(같은 방향)로 끌어온 Content 좌표를 돌려준다. 전체 맵이면 그대로.
	FVector2D ClampToMinimapEdge(const FVector2D& ContentPosition) const;

	FShooterMapPlayerMarker& GetOrCreatePlayerMarker(int32 Index);
	int32 GetOrCreateFreePingIndex();
	UImage* CreateMarkerImage(int32 ZOrder);

	// ---- 설정 ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shooter|Map")
	EShooterMapMode MapMode = EShooterMapMode::Minimap;

	// Panel_MapContent의 설계 크기(px). 배경 텍스처 해상도와 비율만 맞으면 된다(정사각형).
	UPROPERTY(EditAnywhere, Category = "Shooter|Map")
	FVector2D MapContentSize = FVector2D(1024.f, 1024.f);

	// 미니맵 원의 반지름이 월드에서 몇 cm인지. 3000이면 반경 30m가 보인다.
	UPROPERTY(EditAnywhere, Category = "Shooter|Map|Minimap", meta = (ClampMin = "100.0", EditCondition = "MapMode == EShooterMapMode::Minimap"))
	float MinimapViewRadius = 3000.f;

	// true면 내가 보는 방향이 항상 위(발로란트 회전 미니맵), false면 북쪽 고정(서든 기본).
	UPROPERTY(EditAnywhere, Category = "Shooter|Map|Minimap", meta = (EditCondition = "MapMode == EShooterMapMode::Minimap"))
	bool bRotateWithView = false;

	// 미니맵 원 밖에 있는 대상을 원 테두리에 그 방향으로 붙여서 보여준다. 원 안으로 들어오면 실제 위치에 찍힌다.
	UPROPERTY(EditAnywhere, Category = "Shooter|Map|Minimap", meta = (EditCondition = "MapMode == EShooterMapMode::Minimap"))
	bool bClampEnemiesToEdge = true;

	UPROPERTY(EditAnywhere, Category = "Shooter|Map|Minimap", meta = (EditCondition = "MapMode == EShooterMapMode::Minimap"))
	bool bClampPingsToEdge = true;

	UPROPERTY(EditAnywhere, Category = "Shooter|Map|Minimap", meta = (EditCondition = "MapMode == EShooterMapMode::Minimap"))
	bool bClampTeammatesToEdge = false;

	// 테두리에 붙일 때 원 가장자리에서 안쪽으로 얼마나(px) 들일지. 아이콘이 마스크에 잘리지 않게 아이콘 반지름 정도로 둔다.
	UPROPERTY(EditAnywhere, Category = "Shooter|Map|Minimap", meta = (ClampMin = "0.0", EditCondition = "MapMode == EShooterMapMode::Minimap"))
	float EdgeMarkerInset = 8.f;

	// 전체 맵 휠 확대 최대 배율. 1배 = 맵 전체, 확대하면 내 위치 중심으로 보여준다. 닫기 전 배율을 기억해 다음에 그대로 연다.
	UPROPERTY(EditAnywhere, Category = "Shooter|Map|FullMap", meta = (ClampMin = "1.0", EditCondition = "MapMode == EShooterMapMode::FullMap"))
	float FullMapMaxZoom = 4.f;

	// 휠 한 칸당 배율 변화량.
	UPROPERTY(EditAnywhere, Category = "Shooter|Map|FullMap", meta = (ClampMin = "0.05", EditCondition = "MapMode == EShooterMapMode::FullMap"))
	float FullMapZoomStep = 0.5f;

	// 목표 배율로 부드럽게 따라가는 속도. 0이면 즉시 바뀐다.
	UPROPERTY(EditAnywhere, Category = "Shooter|Map|FullMap", meta = (ClampMin = "0.0", EditCondition = "MapMode == EShooterMapMode::FullMap"))
	float FullMapZoomInterpSpeed = 15.f;

	// 위치 샘플링 주기(초). 0.05 = 초당 20회.
	UPROPERTY(EditAnywhere, Category = "Shooter|Map", meta = (ClampMin = "0.016"))
	float PositionUpdateInterval = 0.05f;

	// 팀원 아이콘도 바라보는 방향으로 돌릴지. 화살표 텍스처를 쓸 때 켠다.
	UPROPERTY(EditAnywhere, Category = "Shooter|Map|Style")
	bool bShowTeammateFacing = true;

	// 아이콘은 위쪽을 바라보는 방향으로 그린다. 텍스처가 없으면 틴트 색 사각형으로 나온다.
	// 단, 내 마커는 텍스처를 비워두면 코드가 굵은 화살표를 생성해 넣는다(보는 방향 표시).
	UPROPERTY(EditAnywhere, Category = "Shooter|Map|Style")
	FSlateBrush SelfMarkerBrush;

	// 팀원 마커도 텍스처가 비어 있으면 같은 생성 화살표를 쓴다(bShowTeammateFacing과 함께 켠다).
	UPROPERTY(EditAnywhere, Category = "Shooter|Map|Style")
	bool bUseArrowForTeammates = true;

	UPROPERTY(EditAnywhere, Category = "Shooter|Map|Style")
	FSlateBrush TeammateMarkerBrush;

	UPROPERTY(EditAnywhere, Category = "Shooter|Map|Style")
	FSlateBrush EnemyMarkerBrush;

	UPROPERTY(EditAnywhere, Category = "Shooter|Map|Style")
	FSlateBrush PingLocationBrush;

	UPROPERTY(EditAnywhere, Category = "Shooter|Map|Style")
	FSlateBrush PingEnemyBrush;

	// ---- 바인딩 ----
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> Panel_MapViewport;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> Panel_MapContent;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_MapBackground;

private:
	// 텍스처가 비어 있는 방향 마커 브러시에 코드로 그린 화살표를 넣는다.
	void ApplyGeneratedArrowBrushes();

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> GeneratedArrowTexture;

	UPROPERTY(Transient)
	TArray<FShooterMapPlayerMarker> PlayerMarkers;

	UPROPERTY(Transient)
	TArray<FShooterMapPingMarker> PingMarkers;

	int32 ActivePlayerMarkerCount = 0;

	// 인원 변화는 알려주는 이벤트가 없어서, 위치 갱신 때 수만 비교해 달라지면 재구성한다.
	int32 CachedPlayerArrayNum = INDEX_NONE;

	// 마지막 UpdatePositions의 Content 변환. 새 핑을 다음 주기 전에 바로 배치할 때 쓴다.
	float CurrentContentScale = 1.f;
	float CurrentContentAngle = 0.f;

	// 미니맵 중심(Content 좌표)과 테두리 반지름(Content 단위). 반지름 0이면 끌어오지 않는다(전체 맵).
	FVector2D CurrentViewCenter = FVector2D::ZeroVector;
	float CurrentEdgeRadius = 0.f;

	bool bFullMapOpen = false;
	bool bHasValidView = false;

	// 전체 맵 배율. 휠은 Target만 바꾸고, 위치 갱신 때 Current가 따라간다.
	float FullMapTargetZoom = 1.f;
	float FullMapCurrentZoom = 1.f;

	TWeakObjectPtr<AShooterMinimapBounds> BoundMapBounds;
	TWeakObjectPtr<AShooterPlayerController> BoundPlayerController;
	TArray<TWeakObjectPtr<ABaseTeamPlayerState>> BoundPlayerStates;

	FDelegateHandle MapBoundsChangedHandle;
	FDelegateHandle FullMapOpenChangedHandle;
	FDelegateHandle FullMapZoomInputHandle;
	FTimerHandle PositionUpdateTimerHandle;
};
