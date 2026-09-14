// AugmentCardLayoutController.h
#pragma once

#include "CoreMinimal.h"
#include "AugmentCardLayoutController.generated.h"

class UCurveFloat;

USTRUCT(BlueprintType)
struct FAugmentCardLayoutSettings
{
	GENERATED_BODY()
	
	// ── Hover ──
	UPROPERTY(EditAnywhere, Category = "Hover")
	float HoverFocusScale = 1.08f;
	UPROPERTY(EditAnywhere, Category = "Hover")
	float HoverFocusLift = -24.f;          // 음수가 위로
	UPROPERTY(EditAnywhere, Category = "Hover")
	float HoverSideScale = 0.94f;
	UPROPERTY(EditAnywhere, Category = "Hover")
	float HoverSidePush = 28.f;            // 바깥으로 밀리는 거리
	UPROPERTY(EditAnywhere, Category = "Hover", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HoverSideOpacity = 0.75f;
	// 클수록 빠르다. 속도 기반이라 목표가 바뀌어도 튀지 않는다
	UPROPERTY(EditAnywhere, Category = "Hover", meta = (ClampMin = "1.0"))
	float HoverInterpSpeed = 12.f;

	// ── Outro ──
	UPROPERTY(EditAnywhere, Category = "Outro")
	float OutroSelectedScale = 1.20f;
	UPROPERTY(EditAnywhere, Category = "Outro")
	float OutroRejectedScale = 0.80f;
	UPROPERTY(EditAnywhere, Category = "Outro")
	float OutroRejectedPush = 160.f;
	UPROPERTY(EditAnywhere, Category = "Outro", meta = (ClampMin = "0.05"))
	float OutroDuration = 0.45f;
	
	// Rejected 의 Opacity 를 처음부터 내리면 디졸브가 그냥 Fade Out 으로 묻힌다.
	// 이 비율 이후에만 투명도를 내린다. 1.0 이면 아예 안 내린다
	UPROPERTY(EditAnywhere, Category = "Outro", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OutroRejectedFadeStart = 0.8f;
	// 비어 있으면 선형. 0~1 구간을 쓴다
	UPROPERTY(EditAnywhere, Category = "Outro")
	TObjectPtr<UCurveFloat> OutroCurve;
};

// 카드 한 장의 목표 상태. 컨트롤러가 이것만 계산하고, 화면이 이것만 적용한다
USTRUCT()
struct FAugmentCardLayoutState
{
	GENERATED_BODY()
	
	FVector2D Translation = FVector2D::ZeroVector;
	float Scale = 1.f;
	float Opacity = 1.f;
};

// 각 카드가 얼마나 움직여야 하는가
USTRUCT()
struct FAugmentLayoutBasis
{
	GENERATED_BODY()
	
	// 각 카드가 "카드 줄의 가운데 자리" 까지 가는 데 필요한 RenderTranslation.
	TArray<FVector2D> SelectionTranslations;
	bool bValid = false;
};

UENUM()
enum class EAugmentLayoutMode : uint8
{
	// 아무것도 hover 되지 않은 기본 배치
	Idle,
	// 한 장이 hover 중
	Hover,
	// 선택 아웃트로가 도는 중
	Outro,
};

// 카드 3장의 목표 트랜스폼을 계산하고 보간한다.
struct FPS_API FAugmentCardLayoutController
{
	// 보간하지 않고 즉시 중립 배치로
	void Reset(int32 InNumCards, const FAugmentCardLayoutSettings& Settings);
	
	void SetBasis(const FAugmentLayoutBasis& InBasis,
		const FAugmentCardLayoutSettings& Settings);
	
	// Reset()에서 false로 초기화
	bool HasValidBasis() const;
	
	// -1이면 hover 없음
	void SetHovered(int32 InIndex, const FAugmentCardLayoutSettings& Settings);
	void BeginOutro(int32 InSelectedIndex, const FAugmentCardLayoutSettings& Settings);
	// 이 틱에 아웃트로가 끝났으면 true 한 번 반환
	bool Tick(float DeltaTime, const FAugmentCardLayoutSettings& Settings);
	
	bool IsOutroPlaying() const;
	// IsOutroPlaying()과 다르게 Hold까지 포함한다.
	
	const TArray<FAugmentCardLayoutState>& GetStates() const;
	float GetRejectedProgress() const;
	int32 GetHoveredIndex() const;
	int32 GetSelectedIndex() const;
	int32 GetNumCards() const;
	
private:
	void ComputeTargets(const FAugmentCardLayoutSettings& Settings);
	
	EAugmentLayoutMode Mode = EAugmentLayoutMode::Idle;
	int32 HoveredIndex = INDEX_NONE;
	int32 SelectedIndex = INDEX_NONE;
	float OutroElapsed = 0.f;
	float RejectedProgress = 0.f;
	
	FAugmentLayoutBasis Basis;
	
	TArray<FAugmentCardLayoutState> CurrentStates;
	TArray<FAugmentCardLayoutState> TargetStates;
	
	// 아웃트로 보간의 시작점으로 고른 카드가 hover 상태에서 그대로 이어제갛ㅁ
	TArray<FAugmentCardLayoutState> OutroStartStates;
};

