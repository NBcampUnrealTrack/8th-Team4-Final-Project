#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "GameMode/Types/DominationTypes.h"
#include "DominationSiteTrackerSlotWidget.generated.h"

class SDominationSiteMarker;

// Display data only. The parent tracker remains the only GameState subscriber.
struct FDominationSiteMarkerState
{
	EDominationPointId PointId = EDominationPointId::None;
	bool bActive = false;
	bool bCapturing = false;
	bool bContested = false;
	bool bLocalFocus = false;
	float Progress = 0.0f;
	FLinearColor ProgressColor = FLinearColor::White;
};

/** Presentation parent for the existing WBP_SiteTrackerSlot.
 *  Keeps its named widgets/functions alive for existing BP callers, but replaces
 *  their drawing with one Slate marker. No gameplay state, RPC or widget Tick.
 */
UCLASS(Blueprintable)
class FPS_API UDominationSiteTrackerSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDominationSiteTrackerSlotWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Domination|Refined Style")
	static int32 GetRefinedMotionRevision() { return 43; }

	void PresentSiteState(const FDominationSiteMarkerState& State);
	void PresentCaptureComplete(const FLinearColor& TeamColor);
	void StopPresentationAnimations();

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Refined Style")
	FSlateBrush RefinedMarkerBrush;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Refined Style")
	FSlateFontInfo RefinedLetterFont;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Refined Style", meta = (ClampMin = "24.0", ClampMax = "96.0"))
	float RefinedMarkerExtent = 64.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Refined Style", meta = (ClampMin = "0.3", ClampMax = "3.0", Units = "s"))
	float RefinedPulseSeconds = 1.25f;

	// One smooth expansion/settle for the face, frame and letter. No bounce or undershoot.
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Refined Style", meta = (ClampMin = "1.0", ClampMax = "1.2"))
	float RefinedPulseMaxScale = 1.045f;

	// Readability floor, blended toward full opacity; not a flashing text fade.
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Refined Style", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float RefinedPulseMinOpacity = 0.90f;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Refined Style", meta = (ClampMin = "0.1", ClampMax = "1.0", Units = "s"))
	float RefinedCaptureFlashSeconds = 0.42f;

	// New properties inherit these defaults in the already-installed r4 WBP.
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Refined Style", meta = (ClampMin = "1.0", ClampMax = "1.25"))
	float RefinedActiveScale = 1.14f;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Refined Style", meta = (ClampMin = "0.4", ClampMax = "1.0"))
	float RefinedInactiveOpacity = 0.68f;

	// Outward light travel, separate from face scale. Zero disables travelling waves.
	// Paint uses existing slot padding, capped to 0.70 * extent radius; layout never grows.
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Refined Style", meta = (ClampMin = "0.0", ClampMax = "0.60"))
	float RefinedPulseWaveExpansion = 0.42f;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Refined Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RefinedPulseGlowStrength = 0.72f;

	// Editor preview only; runtime IDs always come from the parent tracker.
	UPROPERTY(EditDefaultsOnly, Category = "Domination|Refined Preview")
	EDominationPointId RefinedPreviewPointId = EDominationPointId::C;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Refined Preview")
	bool bRefinedPreviewActive = true;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;

private:
	void SynchronizeMarker();
	TSharedPtr<SDominationSiteMarker> RefinedMarker;
	FDominationSiteMarkerState RefinedState;
};
