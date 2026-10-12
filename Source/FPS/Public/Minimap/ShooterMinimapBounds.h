#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "ShooterMinimapBounds.generated.h"

class UBoxComponent;
class UTexture2D;

/**
 * 레벨마다 하나 배치하는 미니맵 기준 영역.
 *
 * 박스(정사각형)가 곧 맵 텍스처 전체에 대응한다. 텍스처의 위쪽 = 이 액터의 Forward(+X),
 * 오른쪽 = Right(+Y)라서 액터를 Yaw 회전시키면 맵의 "북쪽"도 같이 돈다.
 *
 * 배경 제작 순서:
 * 1. 레벨 전체를 덮도록 배치하고 MapWorldHalfSize를 맞춘다.
 * 2. 디테일 패널의 ExportLineArt 버튼 → Saved/Minimap/에 선 그림 PNG + SVG가 떨어진다.
 *    (손으로 따고 싶으면 ExportTopDownCapture로 밑그림 PNG를 뽑아 그 위에 그린다.)
 * 3. 필요하면 SVG를 피그마/일러스트레이터에서 다듬어 같은 해상도의 정사각형 PNG로 다시 내보낸다.
 * 4. 임포트한 텍스처를 MapTexture에 넣는다.
 */
UCLASS()
class FPS_API AShooterMinimapBounds : public AActor
{
	GENERATED_BODY()

public:
	AShooterMinimapBounds();

	virtual void OnConstruction(const FTransform& Transform) override;

	// 월드 좌표 → 맵 UV(0~1). 좌상단이 (0,0). 박스 밖이면 0~1 범위를 벗어난다.
	FVector2D WorldToMapUV(const FVector& WorldLocation) const;

	// 월드 Yaw → 맵 위 회전각(도, 시계방향, 위쪽이 0). UMG RenderTransform Angle에 그대로 쓴다.
	float WorldYawToMapAngle(float WorldYaw) const;

	float GetMapWorldHalfSize() const { return MapWorldHalfSize; }
	UTexture2D* GetMapTexture() const { return MapTexture; }
	FVector2D GetPlayableAreaUVMin() const { return PlayableAreaUVMin; }
	FVector2D GetPlayableAreaUVMax() const { return PlayableAreaUVMax; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	// 박스 위에서 내려다본 직교 캡처를 Saved/Minimap/<레벨이름>_TopDown.png로 내보낸다.
	// 선 따기용 밑그림이다. 지붕이 찍히면 캡처 전에 잠시 숨기고 누른다.
	UFUNCTION(CallInEditor, Category = "Shooter|Minimap")
	void ExportTopDownCapture();

	// 박스 윗면에서 격자로 라인트레이스를 쏴 높이맵을 만들고, 높이가 급격히 바뀌는 곳(벽·엄폐물 경계)의
	// 경계를 벡터(곧은 선)로 뽑아 Saved/Minimap/<레벨이름>_Lines.png(바로 MapTexture로 사용)와
	// _Lines.svg(피그마/일러스트레이터 편집용)로 내보낸다.
	// 위치는 월드와 1:1로 정확하다. 위아래로 겹치는 다층 구조는 위층 기준으로만 나온다.
	UFUNCTION(CallInEditor, Category = "Shooter|Minimap")
	void ExportLineArt();
#endif

	UPROPERTY(VisibleAnywhere, Category = "Shooter|Minimap")
	TObjectPtr<UBoxComponent> BoundsBox;

	// 맵 한 변의 절반(cm). 박스가 정사각형으로 고정되어 텍스처 비율과 항상 맞는다.
	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap", meta = (ClampMin = "100.0"))
	float MapWorldHalfSize = 5000.f;

	// 캡처 카메라 높이 기준. 박스 윗면에서 아래를 내려다본다.
	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap", meta = (ClampMin = "100.0"))
	float MapWorldHalfHeight = 2000.f;

	// 선으로 그린 맵 배경. 정사각형 텍스처여야 한다.
	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap")
	TObjectPtr<UTexture2D> MapTexture;

	// 텍스처 중 실제 맵이 그려진 영역(UV, 좌상단 0,0). 가로로 긴 맵이면 위아래 빈 공간을 빼도록 Y를 좁힌다.
	// 전체 맵(M)은 이 영역(또는 그 안의 활성 구역)만 창에 맞춰 보여준다.
	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector2D PlayableAreaUVMin = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector2D PlayableAreaUVMax = FVector2D::UnitVector;

	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap|Capture", meta = (ClampMin = "256", ClampMax = "8192"))
	int32 CaptureResolution = 2048;

	// BaseColor는 조명 영향 없이 평평하게 찍혀서 선 따기에 편하다.
	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap|Capture")
	TEnumAsByte<ESceneCaptureSource> CaptureSource = SCS_BaseColor;

	// 출력 PNG 한 변 픽셀 수.
	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap|LineArt", meta = (ClampMin = "128", ClampMax = "4096"))
	int32 LineArtResolution = 2048;

	// 출력 1px을 N×N번 측정한 뒤 평균 내서 선 가장자리를 부드럽게 만든다(안티에일리어싱).
	// 트레이스 수는 (Resolution×N)²라 2048×2면 약 1,600만 번. 멀티스레드로 돈다.
	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap|LineArt", meta = (ClampMin = "1", ClampMax = "4"))
	int32 LineArtSupersample = 2;

	// 이웃 픽셀과 이 높이(cm) 이상 차이 나면 벽/엄폐물 경계로 보고 선을 긋는다.
	// 계단 한 칸(~20)보다 크고 허리 엄폐물(~100)보다 작게 잡는다.
	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap|LineArt", meta = (ClampMin = "5.0"))
	float WallHeightThreshold = 60.f;

	// 주변 이 반경(cm) 안의 가장 낮은 바닥보다 WallHeightThreshold 이상 높으면 장애물(벽·상자 윗면)로 칠한다.
	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap|LineArt", meta = (ClampMin = "0.0"))
	float ObstacleSearchRadius = 300.f;

	// 출력 PNG 기준 선 두께(px). 슈퍼샘플링 덕에 1.5처럼 소수도 된다(Supersample 단위로 반올림).
	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap|LineArt", meta = (ClampMin = "0.25", ClampMax = "8.0"))
	float LineWidth = 1.f;

	// 직선화 강도(cm). 원래 경계에서 이만큼까지 벗어나도 한 직선으로 합친다.
	// 클수록 벽이 곧고 깔끔하지만 둥근 기둥·곡면이 각져진다. 0이면 직선화 안 함.
	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap|LineArt", meta = (ClampMin = "0.0"))
	float LineArtSimplifyTolerance = 15.f;

	// 둘레가 이보다 짧은(cm) 고리는 버린다. 작은 소품·바닥 노이즈가 점처럼 찍히는 걸 막는다.
	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap|LineArt", meta = (ClampMin = "0.0"))
	float LineArtMinContourLength = 150.f;

	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap|LineArt")
	TEnumAsByte<ECollisionChannel> LineArtTraceChannel = ECC_Visibility;

	// true면 렌더 메시 모양 그대로(정확, 느림), false면 단순 콜리전(빠름, 대략적).
	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap|LineArt")
	bool bLineArtTraceComplex = true;

	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap|LineArt")
	FLinearColor LineArtBackgroundColor = FLinearColor(0.f, 0.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap|LineArt")
	FLinearColor LineArtFloorColor = FLinearColor(0.12f, 0.12f, 0.14f, 0.75f);

	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap|LineArt")
	FLinearColor LineArtObstacleColor = FLinearColor(0.02f, 0.02f, 0.03f, 0.9f);

	UPROPERTY(EditAnywhere, Category = "Shooter|Minimap|LineArt")
	FLinearColor LineArtLineColor = FLinearColor(0.85f, 0.9f, 0.95f, 1.f);
};
