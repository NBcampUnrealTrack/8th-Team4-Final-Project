// DominationClassPreviewStage.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameMode/Types/DominationTypes.h"
#include "DominationClassPreviewStage.generated.h"

class AShooterCharacter;
class AWeaponBase;
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
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PreviewMID;

private:
	void CollectPreviewWeapons();
	void RegisterPreviewComponents();
	void DestroyPreviewPawn();
	void PlayChangeEffect();

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;
	// 교체와 확정은 별개 Niagara System
	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<UNiagaraComponent> ChangeEffect;
	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<UNiagaraComponent> LockInEffect;

	UPROPERTY(Transient)
	TObjectPtr<AShooterCharacter> PreviewPawn;
	TArray<TWeakObjectPtr<AWeaponBase>> PreviewWeapons;

	EDominationCombatClass CurrentPreviewClass = EDominationCombatClass::Unselected;
};
