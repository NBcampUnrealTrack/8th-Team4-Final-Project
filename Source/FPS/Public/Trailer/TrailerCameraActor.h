// 트레일러 촬영용 임시 카메라. 개발/촬영 전용이며 쉬핑 빌드에서는 동작하지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrailerCameraActor.generated.h"

class UCineCameraComponent;
class APawn;

UENUM()
enum class ETrailerCameraMode : uint8
{
	// WASD/QE/마우스로 직접 조종
	Free,
	// 대상 플레이어 뒤에서 따라가며 바라본다
	Follow,
	// 대상 플레이어 주위를 일정 속도로 돈다
	Orbit
};

// 로컬 클라이언트에서만 존재하는 시네마 카메라. 복제하지 않는다.
// 서버에서 관전 상태로 옮겨진 카메라 클라이언트가 이 액터로 시점을 바꿔 촬영한다.
UCLASS(NotPlaceable, Transient)
class FPS_API ATrailerCameraActor : public AActor
{
	GENERATED_BODY()

public:
	ATrailerCameraActor();

	virtual void Tick(float DeltaSeconds) override;

	void SetMode(ETrailerCameraMode NewMode);
	ETrailerCameraMode GetMode() const { return Mode; }

	// 대상 플레이어를 순서대로 바꾼다.
	void CycleTarget(int32 Direction);
	// 대상 플레이어 번호를 직접 지정한다(본인 제외 목록 기준, 0부터).
	void SetTargetIndex(int32 NewIndex);

	// 이름으로 값을 바꾼다. 알 수 없는 이름이면 false.
	bool SetParameter(const FString& Name, float Value);

	// 현재 상태 한 줄 요약(콘솔/화면 표시용)
	FString DescribeState() const;

private:
	// 대상 후보(본인 제외, 살아있는 캐릭터)를 번호 순으로 모은다.
	void GatherTargets(TArray<APawn*>& OutTargets) const;
	APawn* ResolveTarget() const;

	void TickFree(APlayerController* PlayerController, float DeltaSeconds);
	void TickFollow(float DeltaSeconds);
	void TickOrbit(float DeltaSeconds);

	void ApplyLookAt(const FVector& LookAtLocation, float DeltaSeconds, bool bSmooth);
	void UpdateFocus(const FVector& LookAtLocation);

	UPROPERTY()
	TObjectPtr<UCineCameraComponent> CineCamera;

	ETrailerCameraMode Mode = ETrailerCameraMode::Follow;
	int32 TargetIndex = 0;
	float OrbitAngleDegrees = 0.f;

	// 자유 비행
	float MoveSpeed = 600.f;
	float LookSensitivity = 0.35f;

	// 따라가기/궤도 공통
	float Distance = 350.f;
	float Height = 60.f;
	float SideOffset = 0.f;
	float LookHeight = 60.f;
	float PositionSmoothing = 6.f;
	float RotationSmoothing = 8.f;
	float OrbitSpeed = 20.f;

	// 피사계 심도
	bool bDepthOfField = false;
};
