// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UObject/SoftObjectPtr.h"
#include "UI/Loading/ShooterLoadingProgressTracker.h"
#include "LoadingScreenWidget.generated.h"

class UCurveFloat;
class UImage;
class UMaterialInstanceDynamic;
class UProgressBar;
class USoundBase;
class UTextBlock;
class UTexture2D;
class UWidgetAnimation;

struct FShooterLoadingContext;

UCLASS(BlueprintType, Blueprintable)
class FPS_API ULoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 새 로딩 세션
	void ResetLoadingSession();

	// 로딩 컨텍스트의 Thumbnail을 비동기 로드
	void RequestPreviewTexture(const TSoftObjectPtr<UTexture2D>& InThumbnail);
	void ApplyLoadingContext(const FShooterLoadingContext& InContext);

	// 100% 까지 수렴시킨 뒤 CompletionHoldSeconds 만큼 더 보여주고 OnFinished 를 부른다.
	void BeginCompletionExit(FSimpleDelegate OnFinished);

	// 세션 최초 부착에서만 1회
	void BeginEnterAnimation();

	UFUNCTION(BlueprintCallable, Category = "Loading")
	void SetLoadingProgress(float InProgress);

	UFUNCTION(BlueprintPure, Category = "Loading")
	float GetLoadingProgress() const;
	UFUNCTION(BlueprintPure, Category = "Loading")
	float GetDisplayProgress() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Loading")
	void OnLoadingProgressChanged(float InProgress);

	UPROPERTY(EditDefaultsOnly, Category = "Loading")
	FShooterLoadingProgressSettings ProgressSettings;

	// Display == 0 에서 포인터가 0 눈금에 오도록 하는 정렬 오프셋
	UPROPERTY(EditDefaultsOnly, Category = "Loading|Dial")
	float DialStartAngle = 60.f;
	// 부호가 회전 방향이다. 눈금이 시계방향이므로 링은 반시계로 돈다
	UPROPERTY(EditDefaultsOnly, Category = "Loading|Dial")
	float DialTravelAngle = -120.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Loading|Preview")
	FName PreviewTextureParameterName = TEXT("PreviewTexture");
	UPROPERTY(EditDefaultsOnly, Category = "Loading|Preview")
	FName BlurParameterName = TEXT("BlurAmount");
	UPROPERTY(EditDefaultsOnly, Category = "Loading|Preview")
	FName CircleMaskParameterName = TEXT("CircleMaskAmount");

	// 배경은 진행률과 무관하게 계속 흐리다
	UPROPERTY(EditDefaultsOnly, Category = "Loading|Preview")
	float BackdropBlurAmount = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Loading|Preview")
	TObjectPtr<UCurveFloat> SharpnessCurve;

	UPROPERTY(EditDefaultsOnly, Category = "Loading|Preview")
	FName BrightnessParameterName = TEXT("Brightness");

	// 비어 있으면 밝기를 건드리지 않는다. 선형 폴백은 0% 를 완전히 검게 만든다
	UPROPERTY(EditDefaultsOnly, Category = "Loading|Preview")
	TObjectPtr<UCurveFloat> BrightnessCurve;

	// 100% 에 처음 닿는 순간 한 번
	UPROPERTY(EditDefaultsOnly, Category = "Loading|Sound")
	TObjectPtr<USoundBase> CompletionSound;

private:
	void ApplyDisplayProgress();
	void ApplyPreviewTexture(UTexture2D* InTexture);
	void PickSessionTip();
	void ApplyTipVisibility(ESlateVisibility InVisibility);
	void TickCompletionExit(float DeltaSeconds);
	void ExecuteCompletionExitDelegate();

	UFUNCTION()
	void HandleExitAnimationFinished();

	// 텍스트 및 게이지
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> LoadingProgressBar;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LoadingPercentText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Percent;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ModeName;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_ModeRule;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Tip;

	// TIP 라벨과 본문은 한 묶음이다. 패널이 켜고 끄는 단위다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TipPanel;

	// 회전 루트. 렌즈 중심과 이미지 중심이 달라 이미지가 아니라 감싼 박스를 돌린다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SizeBox_FocusRotationRoot;

	// 머터리얼
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Preview;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Backdrop;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PreviewMID;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BackdropMID;

	TSharedPtr<struct FStreamableHandle> PreviewHandle;

	FSoftObjectPath PendingThumbnail;

	// 완료 대기 상태. Tick 이 소유한다
	bool bCompletionExitPending = false;
	float CompletionHoldElapsed = 0.0f;
	float CompletionWaitElapsed = 0.0f;
	FSimpleDelegate CompletionExitDelegate;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Enter;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Exit;

	// 사운드가 이 안에 들어 있다. 100% 소리 전에 멈춘다
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_DecorRailIdle;

	FWidgetAnimationDynamicEvent ExitAnimationFinishedDelegate;

	// Exit 재생 중. 완료 콜백이 이 플래그로만 델리게이트를 쏜다
	bool bExitAnimationPlaying = false;

	// 100% 소리는 세션당 한 번이다
	bool bCompletionSoundPlayed = false;

	FShooterLoadingProgressTracker Tracker;

	float LoadingProgress = 0.0f;
};
