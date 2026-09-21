#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameMode/Types/DominationTypes.h"
#include "DominationCaptureGate.generated.h"

class ADominationGameState;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class FPS_API ADominationCaptureGate : public AActor
{
	GENERATED_BODY()

public:
	ADominationCaptureGate();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Domination|Gate")
	bool IsOpen() const { return bIsOpen; }

protected:
	// 문 상태가 바뀔 때 호출된다. BP 자식에서 Timeline 등으로 실제 열림/닫힘 연출을 구현한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Domination|Gate")
	void OnGateOpenStateChanged(bool bOpen);

private:
	UFUNCTION()
	void HandleActivePointChanged(EDominationPointId ActivePointId);

	UFUNCTION()
	void HandleCaptureStateChanged(FDominationCaptureState CaptureState);

	void RefreshGateState();
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|Gate", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	// 이 문이 반응할 거점. 해당 거점이 활성 상태이고 진행률이 OpenThreshold 이상이면 열린다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Domination|Gate", meta = (AllowPrivateAccess = "true"))
	EDominationPointId PointId = EDominationPointId::C;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Domination|Gate", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float OpenThreshold = 0.5f;

	UPROPERTY(Transient)
	TObjectPtr<ADominationGameState> BoundGameState;

	bool bIsOpen = false;
};
