// DominationCardHoverController.h
#pragma once

#include "CoreMinimal.h"
#include "DominationCardHoverController.generated.h"

USTRUCT(BlueprintType)
struct FDominationCardHoverSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Hover")
	float FocusScale = 1.08f;

	// 음수가 위로
	UPROPERTY(EditAnywhere, Category = "Hover")
	float FocusLift = -24.0f;

	UPROPERTY(EditAnywhere, Category = "Hover")
	float SideScale = 0.94f;

	UPROPERTY(EditAnywhere, Category = "Hover")
	float SidePush = 28.0f;

	UPROPERTY(EditAnywhere, Category = "Hover", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SideOpacity = 0.75f;

	UPROPERTY(EditAnywhere, Category = "Hover", meta = (ClampMin = "1.0"))
	float InterpSpeed = 12.0f;
};

USTRUCT()
struct FDominationCardHoverState
{
	GENERATED_BODY()

	FVector2D Translation = FVector2D::ZeroVector;
	float Scale = 1.0f;
	float Opacity = 1.0f;
};

/**
 * 카드 N장의 목표 트랜스폼을 계산하고 보간한다.
 */
struct FPS_API FDominationCardHoverController
{
	void Reset(int32 InNumCards);
	// INDEX_NONE이면 hover 없음
	void SetHovered(int32 InIndex);
	// 이 틱에 값이 바뀌었으면 true. false면 화면이 적용을 건너뜀
	bool Tick(float DeltaTime, const FDominationCardHoverSettings& Settings);

	const TArray<FDominationCardHoverState>& GetStates() const;
	int32 GetHoveredIndex() const;

private:
	void ComputeTargets(const FDominationCardHoverSettings& Settings);

	int32 HoveredIndex = INDEX_NONE;

	TArray<FDominationCardHoverState> CurrentStates;
	TArray<FDominationCardHoverState> TargetStates;
};
