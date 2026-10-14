//TeammateIndicatorEntry.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "TeammateIndicatorEntry.generated.h"

class AShooterPlayerState;
class UCanvasPanel;
class UCanvasPanelSlot;
class UCommonTextBlock;
class UImage;
class UPanelWidget;

/**
 * 아군 한 명 머리 위 표시하는 화면공간 인디케이터
 * 
 * 매프레임 NativeTick에서 대상의 head 소켓을 화면 좌표로 투영해 배치하고
 * 카메라 -> 머리 라인트레이스로 닉네임만 가림 처리(점/Jack표시는 벽을 뚫음)
 */
UCLASS()
class FPS_API UTeammateIndicatorEntry : public UCommonUserWidget
{
	GENERATED_BODY()
	
public:
	// Layer가 생성 직후 1회 호출
	void InitializeIndicator(UCanvasPanel* InOwnerCanvas, UCanvasPanelSlot* InCanvasSlot);

	// Layer가 로스터 구성 시 호출. nullptr이면 Collapsed
	void SetPlayerState(AShooterPlayerState* NewPlayerState);
	
	// Layer가 Jack 캐리어 판정 결과를 넣는다.
	void SetJackCarrierVisible(bool bVisible);
	
	AShooterPlayerState* GetBoundPlayerState() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	
	// 투영 -> 범위 판정 -> 배치 -> 오쿨루전 트레이스 순서
	void UpdateScreenPositionAndVisibility();
	
	// 대상 head 소켓 월드 위치. Pawn/Mesh가 없으면 false 반환
	bool GetTargetHeadLocation(FVector& OutHeadLocation, AActor*& OutTargetActor) const;
	
	// BoundPlayerState에 생사 델리게이트를 건다.
	// SetPlayerState와NativeConstruct(재구축) 양쪽에서 호출
	void BindToPlayerState();
	
	// 현재 대상의 생사 구독을 끊는다.
	void UnbindFromPlayerState();
	
	// PlayerState의 생사 변화 구독
	UFUNCTION()
	void HandleAliveStateChanged(bool bNewIsAlive);
	
	// 카메라와 대상 머리 사이에 시야를 막는 오브젝트가 있는지 확인한다.
	// 벽이나 엄폐물에 가려져 있으면 false를 반환한다.
	bool HasLineOfSightToTargetHead(const FVector& HeadLocation, const AActor* TargetActor) const;
	
	// 화면 밖/사망/Pawn없음 상태일 때 이 패널만 Collapsed
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> Panel_IndicatorVisual;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Nickname;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_FriendlyDot;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_JackCarrier;
	
	// 머리 소켓 이름
	UPROPERTY(EditDefaultsOnly, Category = "Shooter|UI")
	FName HeadSocketName = TEXT("head");
	
	// 머리 소켓 위로 얼마나 띄울지
	UPROPERTY(EditDefaultsOnly, Category = "Shooter|UI")
	float HeadWorldZOffset = 30.f;
	
	// 라인트레이스 채널
	UPROPERTY(EditDefaultsOnly, Category = "Shooter|UI")
	TEnumAsByte<ECollisionChannel> VisibilityTraceChannel = ECC_Visibility;
	
	// 닉네임 가림 트레이스 간격(초). 0이면 매 프레임
	UPROPERTY(EditDefaultsOnly, Category = "Shooter|UI", meta = (ClampMin = "0.0"))
	float LineOfSightCheckInterval = 0.1f;
	
private:
	TWeakObjectPtr<AShooterPlayerState> BoundPlayerState;
	
	// 매프레임 GetParent() 캐스팅 피하기 위함 캐싱
	TWeakObjectPtr<UCanvasPanel> OwnerCanvas;
	TWeakObjectPtr<UCanvasPanelSlot> CachedCanvasSlot;
	
	bool bCachedIsAlive = false;
	
	// 다음 가림 트레이스 월드 시각. 0이면 다음 표시 프레임에 바로 검사
	double NextLineOfSightCheckTime = 0.0;
	bool bCachedHeadVisible = false;
};
