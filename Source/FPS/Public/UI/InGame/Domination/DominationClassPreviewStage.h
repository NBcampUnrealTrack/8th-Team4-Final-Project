// DominationClassPreviewStage.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameMode/Types/DominationTypes.h"
#include "DominationClassPreviewStage.generated.h"

class AShooterCharacter;
class AWeaponBase;
class UAnimMontage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNiagaraComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/**
 * 클래스 선택 화면의 캐릭터 프리뷰 스테이지
 * 프리뷰 Pawn을 비추고 렌더 타겟에 담아 UI가 쓸 머티리얼로 넘김
 */
UCLASS(Abstract, Blueprintable)
class FPS_API ADominationClassPreviewStage : public AActor
{
	GENERATED_BODY()

public:
	ADominationClassPreviewStage();

	void SetPreviewClass(EDominationCombatClass CombatClass);
	void PlayLockInEffect();
	UMaterialInstanceDynamic* GetPreviewMaterial() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	TObjectPtr<UMaterialInterface> PreviewMaterialTemplate;

	// 프리뷰 렌더 타겟 해상도. 
	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	FIntPoint RenderTargetSize = FIntPoint(256, 512);
	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	TEnumAsByte<ETextureRenderTargetFormat> RenderTargetFormat = RTF_RGBA16f;

	// 끄면 클래스가 바뀔 때만 한 번 캡처한다. 대기 애니메이션은 멈춘 것처럼 보인다.
	UPROPERTY(EditDefaultsOnly, Category = "Preview")
	bool bCaptureEveryFrame = true;

	// 클래스가 바뀔 때 한 번 재생
	UPROPERTY(EditDefaultsOnly, Category = "Preview|Anim")
	TMap<EDominationCombatClass, TObjectPtr<UAnimMontage>> DrawMontages;
	// 대기 중 간간이 재생
	UPROPERTY(EditDefaultsOnly, Category = "Preview|Anim")
	TMap<EDominationCombatClass, TObjectPtr<UAnimMontage>> InspectMontages;
	// 이전 몽타주가 끝난 뒤 다음 Inspect까지 대기(초)
	UPROPERTY(EditDefaultsOnly, Category = "Preview|Anim")
	FVector2D InspectDelayRange = FVector2D(4.0f, 8.0f);
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PreviewMID;

private:
	void SpawnPreviewPawns();
	AShooterCharacter* SpawnPreviewPawn(EDominationCombatClass CombatClass);
	bool RequestSavedAppearance(EDominationCombatClass CombatClass, AShooterCharacter* Pawn);
	void PollAppearanceApplied();
	void ShowCurrentPreview();
	void RegisterPreviewComponents();
	void DestroyPreviewPawns();
	void KeepPreviewTexturesStreamed();
	void PlayChangeEffect();
	float PlayPreviewMontage(const TMap<EDominationCombatClass, TObjectPtr<UAnimMontage>>& Montages) const;
	void ScheduleInspect(float MontageDuration);
	void PlayInspect();

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;
	// 교체와 확정은 별개 Niagara System
	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<UNiagaraComponent> ChangeEffect;
	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<UNiagaraComponent> LockInEffect;

	// 클래스마다 한 번만 스폰해 두고 전환은 캡처 대상만 바꾼다.
	UPROPERTY(Transient)
	TMap<EDominationCombatClass, TObjectPtr<AShooterCharacter>> PreviewPawns;
	// 지금 보여주는 클래스의 Pawn
	UPROPERTY(Transient)
	TObjectPtr<AShooterCharacter> PreviewPawn;
	// 정리용. 모든 프리뷰 Pawn의 무기
	TArray<TWeakObjectPtr<AWeaponBase>> PreviewWeapons;
	// 외형 생성이 끝나지 않아 아직 캡처하지 않는 클래스
	TSet<EDominationCombatClass> PendingAppearanceClasses;
	FTimerHandle InspectTimerHandle;
	FTimerHandle AppearanceTimerHandle;
	FTimerHandle StreamingTimerHandle;
	int32 AppearancePollsRemaining = 0;

	EDominationCombatClass CurrentPreviewClass = EDominationCombatClass::Unselected;
};
