#include "UI/InGame/HUD/DominationSiteTrackerSlotWidget.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"

// r4.3: smooth shared motion, broad soft ripples and a short release. Existing layout stays unchanged.
namespace
{
	FLinearColor WithAlpha(FLinearColor Color, const float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}

	FString PointLetter(const EDominationPointId PointId)
	{
		const int32 Index = DominationRules::GetPointIndex(PointId);
		return Index == INDEX_NONE ? FString() : FString::Chr(static_cast<TCHAR>(TEXT('A') + Index));
	}


	float EaseOutCubic(const float Value)
	{
		const float Inv = 1.0f - FMath::Clamp(Value, 0.0f, 1.0f);
		return 1.0f - Inv * Inv * Inv;
	}

	float SafeStyleValue(const float Value, const float Default, const float Min, const float Max)
	{
		return FMath::Clamp(FMath::IsFinite(Value) ? Value : Default, Min, Max);
	}

	float Smoother01(const float Value)
	{
		const float T = FMath::Clamp(Value, 0.0f, 1.0f);
		return T * T * T * (T * (T * 6.0f - 15.0f) + 10.0f);
	}

	struct FMarkerPulse
	{
		float Lift = 0.0f;
		float Light = 0.0f;
		float WaveTravel[2] = {0.0f, 0.0f};
		float WaveAlpha[2] = {0.0f, 0.0f};
	};

	FMarkerPulse EvaluatePulse(const double Cycles)
	{
		FMarkerPulse Result;
		const double Elapsed = FMath::Max(0.0, Cycles);
		const float T = static_cast<float>(FMath::Fmod(Elapsed, 1.0));
		// C2-continuous rise/settle: no anticipation, recoil or abrupt cycle reset.
		const float Rise = Smoother01(T / 0.28f);
		const float Settle = 1.0f - Smoother01((T - 0.28f) / 0.62f);
		Result.Lift = Rise * Settle;
		Result.Light = Smoother01(T / 0.24f)
			* (1.0f - Smoother01((T - 0.25f) / 0.69f));
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const double Start = Index == 0 ? 0.13 : 0.39;
			const float Duration = Index == 0 ? 0.94f : 0.88f;
			if (Elapsed < Start) continue; // Never synthesize a wave before the first capture.
			const float Age = static_cast<float>(FMath::Fmod(Elapsed - Start, 1.0));
			if (Age >= Duration) continue;
			const float U = Age / Duration;
			Result.WaveTravel[Index] = 0.35f * U + 0.65f * Smoother01(U);
			Result.WaveAlpha[Index] = Smoother01(U / 0.18f)
				* (1.0f - Smoother01((U - 0.18f) / 0.82f))
				* (Index == 0 ? 1.0f : 0.46f);
		}
		return Result;
	}
}

class SDominationSiteMarker final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SDominationSiteMarker) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		SetCanTick(false);
		SetVisibility(EVisibility::HitTestInvisible);
		SetClipping(EWidgetClipping::Inherit);
	}

	void Configure(const UDominationSiteTrackerSlotWidget& Owner)
	{
		FrameBrush = Owner.RefinedMarkerBrush;
		Font = Owner.RefinedLetterFont;
		Extent = SafeStyleValue(Owner.RefinedMarkerExtent, 64.0f, 24.0f, 96.0f);
		PulseSeconds = SafeStyleValue(Owner.RefinedPulseSeconds, 1.25f, 0.3f, 3.0f);
		PulseMaxScale = SafeStyleValue(Owner.RefinedPulseMaxScale, 1.045f, 1.0f, 1.2f);
		PulseMinOpacity = SafeStyleValue(Owner.RefinedPulseMinOpacity, 0.90f, 0.5f, 1.0f);
		FlashSeconds = SafeStyleValue(Owner.RefinedCaptureFlashSeconds, 0.42f, 0.1f, 1.0f);
		ActiveScale = SafeStyleValue(Owner.RefinedActiveScale, 1.14f, 1.0f, 1.25f);
		InactiveOpacity = SafeStyleValue(Owner.RefinedInactiveOpacity, 0.68f, 0.4f, 1.0f);
		WaveExpansion = SafeStyleValue(Owner.RefinedPulseWaveExpansion, 0.42f, 0.0f, 0.60f);
		GlowStrength = SafeStyleValue(Owner.RefinedPulseGlowStrength, 0.72f, 0.0f, 1.0f);
		bAnimateActivation = !Owner.IsDesignTime();
		bMeasureDirty = true;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void Present(const FDominationSiteMarkerState& NewState)
	{
		const bool bIdentityChanged = State.PointId != NewState.PointId;
		const bool bBecameActive = NewState.bActive && (!State.bActive || bIdentityChanged);
		const bool bNewPulse = NewState.bActive && NewState.bCapturing && !NewState.bContested;
		const bool bChanged = bIdentityChanged || State.bActive != NewState.bActive
			|| State.bCapturing != NewState.bCapturing || State.bContested != NewState.bContested
			|| State.bLocalFocus != NewState.bLocalFocus
			|| !FMath::IsNearlyEqual(State.Progress, NewState.Progress)
			|| !State.ProgressColor.Equals(NewState.ProgressColor);
		const double Now = FSlateApplication::Get().GetCurrentTime();
		if (bIdentityChanged)
		{
			StopAnimations();
			Label = PointLetter(NewState.PointId);
			bMeasureDirty = true;
		}
		State = NewState;
		State.Progress = SafeStyleValue(State.Progress, 0.0f, 0.0f, 1.0f);
		if (bBecameActive && bAnimateActivation)
		{
			ActivationStartTime = Now;
			ActivationAlpha = 0.0f;
			bActivating = true;
		}
		else if (!State.bActive)
		{
			ActivationAlpha = 1.0f;
			bActivating = false;
		}
		if (bNewPulse) PulseColor = NewState.ProgressColor;
		if (bPulsing != bNewPulse)
		{
			if (bNewPulse)
			{
				bPulsing = true;
				bReleasing = false;
				PulseStartTime = Now;
				Pulse = FMarkerPulse();
			}
			else
			{
				BeginRelease(Now);
			}
		}
		// Progress/color updates do not restart a pulse or register a second timer.
		UpdateAnimationTimer();
		if (bChanged) Invalidate(EInvalidateWidgetReason::Paint);
	}

	void CaptureComplete(const FLinearColor& Color)
	{
		FlashColor = Color;
		FlashStartTime = FSlateApplication::Get().GetCurrentTime();
		FlashAlpha = 0.0f;
		bFlashing = true;
		if (bPulsing) BeginRelease(FlashStartTime);
		UpdateAnimationTimer();
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void StopAnimations()
	{
		bPulsing = false;
		bReleasing = false;
		bFlashing = false;
		bActivating = false;
		Pulse = FMarkerPulse();
		FlashAlpha = ActivationAlpha = 1.0f;
		UpdateAnimationTimer();
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	// The hidden legacy subtree preserves its authored desired size/slot math.
	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D::ZeroVector;
	}

	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
		FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override
	{
		const FVector2f Size(Geometry.GetLocalSize());
		if (Label.IsEmpty() || Size.X < 16.0f || Size.Y < 16.0f) return Layer;

		const FVector2f Center = Size * 0.5f;
		const float SafeRadius = 0.5f * FMath::Min(Size.X, Size.Y) - 2.0f;
		const float Diameter = FMath::Min(Extent, SafeRadius * 2.0f);
		const float Unit = Diameter / 64.0f;
		const float RingRadius = FMath::Min(Diameter * 0.50f, SafeRadius - 8.0f * Unit);
		const float BaseRadius = FMath::Min(Diameter * 0.345f,
			(RingRadius - 2.5f * Unit) / (PulseMaxScale * ActiveScale));
		const float RestRadius = BaseRadius * (State.bActive ? ActiveScale : 0.97f);
		const float ActivationScale = bActivating ? FMath::Lerp(0.98f, 1.0f, Smoother01(ActivationAlpha)) : 1.0f;
		const float SharedScale = (1.0f + (PulseMaxScale - 1.0f) * Pulse.Lift) * ActivationScale;
		const float FrameRadius = RestRadius * SharedScale;
		const float FrameSize = FrameRadius / 0.385f;
		const float Opacity = State.bActive
			? FMath::Lerp(PulseMinOpacity, 1.0f, 0.70f + 0.30f * Pulse.Light) : InactiveOpacity;
		const float Energy = Pulse.Light * GlowStrength;
		// Use the existing 20px side padding, not a larger desired size or viewport.
		// Only the light overdraws the 64px slot; the frame/progress keep their size.
		const float WaveLimit = FMath::Max(RestRadius + Unit,
			FMath::Min(RestRadius + Diameter * WaveExpansion, Diameter * 0.70f));
		// Prevent one-pixel stepping while the shared face/text transform interpolates.
		const ESlateDrawEffect Effect = (bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect)
			| ESlateDrawEffect::NoPixelSnapping;
		const FLinearColor GlobalTint = Style.GetColorAndOpacityTint();
		const FLinearColor White(0.91f, 0.94f, 0.97f, 1.0f);
		const FLinearColor Shadow(0.002f, 0.004f, 0.006f, 0.88f);
		const FLinearColor ProgressColor = State.bContested
			? FLinearColor(0.84f, 0.54f, 0.21f, 1.0f) : State.ProgressColor;
		const FLinearColor Accent = State.bContested ? ProgressColor :
			FMath::Lerp(White, PulseColor, 0.58f * Pulse.Light);
		const FLinearColor Edge = State.bActive ? FMath::Lerp(White, Accent, 0.35f + 0.40f * Energy)
			: FLinearColor(0.37f, 0.42f, 0.48f, 0.68f);

		auto DrawPolyline = [&](TArray<FVector2f> Points, const FLinearColor& Color, const float Thickness)
		{
			if (Color.A <= 0.002f || Points.Num() < 2) return;
			FSlateDrawElement::MakeLines(Out, ++Layer, Geometry.ToPaintGeometry(), MoveTemp(Points),
				Effect, Color * GlobalTint, true, Thickness);
		};
		auto DrawDiamond = [&](const float Radius, const FLinearColor& Color, const float Thickness)
		{
			TArray<FVector2f> Points;
			Points.Reserve(5);
			const FVector2f Start = Center + FVector2f(0.0f, -Radius);
			Points.Add(Start);
			Points.Add(Center + FVector2f(Radius, 0.0f));
			Points.Add(Center + FVector2f(0.0f, Radius));
			Points.Add(Center + FVector2f(-Radius, 0.0f));
			Points.Add(Start); // Independent value: never Add(Points[0]).
			DrawPolyline(MoveTemp(Points), Color, Thickness);
		};
		auto DrawArc = [&](const float Radius, const float Start, const float Fraction,
			const FLinearColor& Color, const float Thickness)
		{
			if (Fraction <= 0.0f || Color.A <= 0.002f) return;
			const int32 Segments = FMath::Max(2, FMath::CeilToInt(64.0f * Fraction));
			TArray<FVector2f> Points;
			Points.Reserve(Segments + 1);
			for (int32 Index = 0; Index <= Segments; ++Index)
			{
				const float Angle = -0.5f * PI + 2.0f * PI * (Start + Fraction * static_cast<float>(Index) / Segments);
				Points.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
			}
			DrawPolyline(MoveTemp(Points), Color, Thickness);
		};

		auto DrawSoftWave = [&](const float Radius, const float Travel, const FLinearColor& Color)
		{
			if (Color.A <= 0.003f) return;
			constexpr int32 Segments = 48;
			const float Roundness = Smoother01(Travel) * 0.82f;
			TArray<FVector2f> Points;
			Points.Reserve(Segments + 1);
			for (int32 Index = 0; Index <= Segments; ++Index)
			{
				const float Angle = 2.0f * PI * static_cast<float>(Index) / Segments;
				const FVector2f Direction(FMath::Cos(Angle), FMath::Sin(Angle));
				const float DiamondRadius = Radius / (FMath::Abs(Direction.X) + FMath::Abs(Direction.Y));
				Points.Add(Center + Direction * FMath::Lerp(DiamondRadius, Radius, Roundness));
			}
			// Soft broad band + medium falloff + a restrained moving edge, all untextured.
			const float Width = FMath::Lerp(7.0f, 3.0f, Travel) * Unit;
			DrawPolyline(Points, WithAlpha(Color, 0.085f), Width);
			DrawPolyline(Points, WithAlpha(Color, 0.18f), Width * 0.48f);
			DrawPolyline(MoveTemp(Points), WithAlpha(Color, 0.55f), FMath::Lerp(1.1f, 0.6f, Travel) * Unit);
		};

		// Emitted behind the face. Ripples cross cycle boundaries without a reset/pop.
		if (State.bActive && WaveExpansion > 0.0f)
		{
			for (int32 Index = 0; Index < 2; ++Index)
			{
				const float Travel = Pulse.WaveTravel[Index];
				const float Radius = FMath::Lerp(RestRadius, WaveLimit, Travel);
				DrawSoftWave(Radius, Travel, WithAlpha(PulseColor, Pulse.WaveAlpha[Index] * GlowStrength));
			}
		}

		// Existing small PNG; an extra dark underlay separates the active face from bright scenery.
		if (FrameBrush.GetResourceObject())
		{
			const auto FaceGeometry = Geometry.ToPaintGeometry(FVector2f(FrameSize, FrameSize),
				FSlateLayoutTransform(Center - FVector2f(FrameSize * 0.5f, FrameSize * 0.5f)));
			if (State.bActive)
			{
				FSlateDrawElement::MakeBox(Out, ++Layer, FaceGeometry, &FrameBrush, Effect,
					FLinearColor(0.10f, 0.13f, 0.17f, 0.90f) * GlobalTint);
			}
			FSlateDrawElement::MakeBox(Out, ++Layer, FaceGeometry, &FrameBrush, Effect,
				WithAlpha(FLinearColor::White, Opacity) * GlobalTint);
		}

		if (State.bActive)
		{
			DrawDiamond(FrameRadius, WithAlpha(Accent, 0.035f + Energy * 0.10f), 7.0f * Unit);
			DrawDiamond(FrameRadius, WithAlpha(Accent, 0.06f + Energy * 0.20f), 3.8f * Unit);
			DrawDiamond(FrameRadius, Shadow, 3.0f * Unit);
		}
		DrawDiamond(FrameRadius, WithAlpha(Edge, Opacity), (State.bActive ? 1.65f : 0.75f) * Unit);

		if (State.bActive)
		{
			DrawDiamond(FMath::Max(Unit, FrameRadius - 2.7f * Unit), WithAlpha(Accent, 0.22f + Energy * 0.18f), 0.65f * Unit);
			// Short side brackets remain visible at zero progress and during contested capture.
			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				const float X = Center.X + Side * (RestRadius + 3.0f * Unit);
				TArray<FVector2f> Bracket;
				Bracket.Reserve(3);
				Bracket.Add(FVector2f(X - Side * 3.0f * Unit, Center.Y - 4.0f * Unit));
				Bracket.Add(FVector2f(X, Center.Y));
				Bracket.Add(FVector2f(X - Side * 3.0f * Unit, Center.Y + 4.0f * Unit));
				DrawPolyline(Bracket, WithAlpha(Shadow, 0.85f), 3.2f * Unit);
				DrawPolyline(MoveTemp(Bracket), WithAlpha(Accent, 0.80f + 0.20f * Energy), 1.25f * Unit);
			}

			const bool bShowProgress = State.Progress > 0.0f || State.bCapturing || State.bContested;
			if (bShowProgress)
			{
				DrawArc(RingRadius, 0.0f, 1.0f, WithAlpha(Shadow, 0.65f), 3.0f * Unit);
				DrawArc(RingRadius, 0.0f, 1.0f, FLinearColor(0.34f, 0.42f, 0.50f, 0.28f), 1.2f * Unit);
				DrawArc(RingRadius, 0.0f, State.Progress, WithAlpha(ProgressColor, 0.10f + 0.14f * Energy), 5.0f * Unit);
				DrawArc(RingRadius, 0.0f, State.Progress, ProgressColor, 2.0f * Unit);
				if (State.Progress > 0.005f && State.Progress < 0.995f)
				{
					// Highlight the true endpoint; the animation never invents progress.
					const float Head = FMath::Min(State.Progress, 0.025f);
					DrawArc(RingRadius, State.Progress - Head, Head, WithAlpha(White, 0.65f + 0.35f * Energy), 2.0f * Unit);
				}
			}
			else
			{
				// Quiet broken halo identifies the active objective even outside its capture zone.
				for (int32 Index = 0; Index < 4; ++Index)
				{
					DrawArc(RingRadius, 0.075f + Index * 0.25f, 0.10f, WithAlpha(Shadow, 0.55f), 2.8f * Unit);
					DrawArc(RingRadius, 0.075f + Index * 0.25f, 0.10f, WithAlpha(White, 0.40f), 1.0f * Unit);
				}
			}

			if (State.bLocalFocus)
			{
				TArray<FVector2f> Focus;
				Focus.Add(Center + FVector2f(-4.0f * Unit, RingRadius + 4.0f * Unit));
				Focus.Add(Center + FVector2f(4.0f * Unit, RingRadius + 4.0f * Unit));
				DrawPolyline(Focus, Shadow, 3.2f * Unit);
				DrawPolyline(MoveTemp(Focus), WithAlpha(Accent, 0.9f), 1.4f * Unit);
			}
		}
		if (bFlashing)
		{
			const float Travel = EaseOutCubic(FlashAlpha);
			const float Fade = (1.0f - FlashAlpha) * (1.0f - FlashAlpha);
			const float Radius = FMath::Lerp(RestRadius, FMath::Min(RestRadius + Diameter * 0.23f, SafeRadius - 3.0f * Unit), Travel);
			DrawDiamond(Radius, WithAlpha(FlashColor, Fade * 0.12f), 6.0f * Unit);
			DrawDiamond(Radius, WithAlpha(FlashColor, Fade), 1.8f * Unit);
		}

		if (bMeasureDirty)
		{
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			CachedTextSize = FVector2f(Measure->Measure(Label, Font));
			bMeasureDirty = false;
		}
		// Keep the letter steady; the face, outline and outward ripples still animate.
		const float LabelFit = FMath::Min(1.0f, Diameter / Extent);
		const float TextLimit = FMath::Max(1.0f, CachedTextSize.X + CachedTextSize.Y);
		const float LabelScale = FMath::Min(LabelFit * (State.bActive ? 1.04f : 0.94f),
			RestRadius * 1.45f / TextLimit);
		const float LabelOpacity = State.bActive ? 1.0f : FMath::Min(1.0f, InactiveOpacity + 0.18f);
		const FVector2f TextOrigin = Center - CachedTextSize * (0.5f * LabelScale);
		FSlateDrawElement::MakeText(Out, ++Layer,
			Geometry.ToPaintGeometry(CachedTextSize, FSlateLayoutTransform(LabelScale, TextOrigin + FVector2f(0.0f, Unit))),
			Label, Font, Effect, WithAlpha(Shadow, LabelOpacity) * GlobalTint);
		FSlateDrawElement::MakeText(Out, ++Layer,
			Geometry.ToPaintGeometry(CachedTextSize, FSlateLayoutTransform(LabelScale, TextOrigin)),
			Label, Font, Effect, WithAlpha(White, LabelOpacity) * GlobalTint);
		return Layer;
	}

private:
	void BeginRelease(const double Now)
	{
		bPulsing = false;
		bReleasing = true;
		ReleaseStartTime = Now;
		ReleasePulse = Pulse;
	}

	void UpdateAnimationTimer()
	{
		if (bPulsing || bReleasing || bFlashing || bActivating)
		{
			if (!AnimationTimer.IsValid())
			{
				AnimationTimer = RegisterActiveTimer(0.0f,
					FWidgetActiveTimerDelegate::CreateSP(this, &SDominationSiteMarker::AdvanceAnimation));
			}
		}
		else if (const TSharedPtr<FActiveTimerHandle> Handle = AnimationTimer.Pin())
		{
			UnRegisterActiveTimer(Handle.ToSharedRef());
			AnimationTimer.Reset();
		}
	}

	EActiveTimerReturnType AdvanceAnimation(double CurrentTime, float)
	{
		if (bPulsing)
		{
			const double Elapsed = FMath::Max(0.0, CurrentTime - PulseStartTime);
			Pulse = EvaluatePulse(Elapsed / PulseSeconds);
		}
		else if (bReleasing)
		{
			const float T = FMath::Clamp(static_cast<float>((CurrentTime - ReleaseStartTime) / 0.22), 0.0f, 1.0f);
			const float Ease = Smoother01(T);
			Pulse = ReleasePulse;
			Pulse.Lift *= 1.0f - Ease;
			Pulse.Light *= 1.0f - Ease;
			for (int32 Index = 0; Index < 2; ++Index)
			{
				Pulse.WaveTravel[Index] = FMath::Lerp(ReleasePulse.WaveTravel[Index], 1.0f, Ease);
				Pulse.WaveAlpha[Index] *= 1.0f - Ease;
			}
			bReleasing = T < 1.0f;
		}
		else
		{
			Pulse = FMarkerPulse();
		}
		if (bFlashing)
		{
			FlashAlpha = FMath::Clamp(static_cast<float>((CurrentTime - FlashStartTime) / FlashSeconds), 0.0f, 1.0f);
			bFlashing = FlashAlpha < 1.0f;
		}
		if (bActivating)
		{
			ActivationAlpha = FMath::Clamp(static_cast<float>((CurrentTime - ActivationStartTime) / 0.20), 0.0f, 1.0f);
			bActivating = ActivationAlpha < 1.0f;
		}
		Invalidate(EInvalidateWidgetReason::Paint);
		if (!bPulsing && !bReleasing && !bFlashing && !bActivating)
		{
			AnimationTimer.Reset();
			return EActiveTimerReturnType::Stop;
		}
		return EActiveTimerReturnType::Continue;
	}

	FDominationSiteMarkerState State;
	FSlateBrush FrameBrush;
	FSlateFontInfo Font;
	FString Label;
	FLinearColor FlashColor = FLinearColor::White;
	FLinearColor PulseColor = FLinearColor::White;
	TWeakPtr<FActiveTimerHandle> AnimationTimer;
	double PulseStartTime = 0.0;
	double FlashStartTime = 0.0;
	double ActivationStartTime = 0.0;
	double ReleaseStartTime = 0.0;
	float Extent = 64.0f;
	float PulseSeconds = 1.25f;
	float PulseMaxScale = 1.045f;
	float PulseMinOpacity = 0.90f;
	float FlashSeconds = 0.42f;
	float ActiveScale = 1.14f;
	float InactiveOpacity = 0.68f;
	float WaveExpansion = 0.42f;
	float GlowStrength = 0.72f;
	float FlashAlpha = 1.0f;
	float ActivationAlpha = 1.0f;
	FMarkerPulse Pulse;
	FMarkerPulse ReleasePulse;
	mutable FVector2f CachedTextSize = FVector2f::ZeroVector;
	mutable bool bMeasureDirty = true;
	bool bPulsing = false;
	bool bReleasing = false;
	bool bFlashing = false;
	bool bActivating = false;
	bool bAnimateActivation = true;
};

UDominationSiteTrackerSlotWidget::UDominationSiteTrackerSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RefinedLetterFont = FCoreStyle::GetDefaultFontStyle("Regular", 18);
	RefinedMarkerBrush.DrawAs = ESlateBrushDrawType::Image;
	RefinedMarkerBrush.ImageSize = FVector2f(64.0f, 64.0f);
}

TSharedRef<SWidget> UDominationSiteTrackerSlotWidget::RebuildWidget()
{
	const TSharedRef<SWidget> LegacyRoot = Super::RebuildWidget();
	SAssignNew(RefinedMarker, SDominationSiteMarker);
	SynchronizeMarker();
	// Hidden, not Collapsed: original sizing and BP object identities survive.
	// The outer wrapper prevents BP opacity/visibility changes from redrawing old art.
	return SNew(SOverlay)
		+ SOverlay::Slot()[SNew(SBox).Visibility(EVisibility::Hidden)[LegacyRoot]]
		+ SOverlay::Slot()[RefinedMarker.ToSharedRef()];
}

void UDominationSiteTrackerSlotWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	SynchronizeMarker();
}

void UDominationSiteTrackerSlotWidget::SynchronizeMarker()
{
	if (!RefinedMarker.IsValid()) return;
	RefinedMarker->Configure(*this);
	FDominationSiteMarkerState State = RefinedState;
	if (IsDesignTime())
	{
		State.PointId = RefinedPreviewPointId;
		State.bActive = bRefinedPreviewActive;
		State.bCapturing = false;
	}
	RefinedMarker->Present(State);
}

void UDominationSiteTrackerSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	SynchronizeMarker();
}

void UDominationSiteTrackerSlotWidget::PresentSiteState(const FDominationSiteMarkerState& State)
{
	RefinedState = State;
	// Legacy per-point scale/tint is now owned by the new presentation.
	SetRenderOpacity(1.0f);
	SetRenderScale(FVector2D(1.0, 1.0));
	if (RefinedMarker.IsValid()) RefinedMarker->Present(State);
}

void UDominationSiteTrackerSlotWidget::PresentCaptureComplete(const FLinearColor& TeamColor)
{
	if (RefinedMarker.IsValid()) RefinedMarker->CaptureComplete(TeamColor);
}

void UDominationSiteTrackerSlotWidget::StopPresentationAnimations()
{
	if (RefinedMarker.IsValid()) RefinedMarker->StopAnimations();
}

void UDominationSiteTrackerSlotWidget::NativeDestruct()
{
	StopPresentationAnimations();
	Super::NativeDestruct();
}

void UDominationSiteTrackerSlotWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	StopPresentationAnimations();
	RefinedMarker.Reset();
	Super::ReleaseSlateResources(bReleaseChildren);
}
