// AugmentCardLayoutController.cpp
#include "UI/InGame/AugmentCardLayoutController.h"

#include "Curves/CurveFloat.h"

void FAugmentCardLayoutController::ComputeTargets(const FAugmentCardLayoutSettings& Settings)
{
	const int32 Num = CurrentStates.Num();
	TargetStates.SetNum(Num);
	
	// 화면 중앙 기준이면 3장 중 가운데 카드는 Sign()==0 이라 안 밀린다.
	const int32 Pivot = (Mode == EAugmentLayoutMode::Hover) ? HoveredIndex : SelectedIndex;
	
	for (int32 i = 0; i < Num; ++i)
	{
		FAugmentCardLayoutState& S = TargetStates[i];
		S = FAugmentCardLayoutState();
		
		const float Direction = FMath::Sign(static_cast<float>(i - Pivot));

		switch (Mode)
		{
		case EAugmentLayoutMode::Hover:
			if (i == HoveredIndex)
			{
				S.Scale = Settings.HoverFocusScale;
				S.Translation.Y = Settings.HoverFocusLift;
			}
			else
			{
				S.Scale = Settings.HoverSideScale;
				S.Translation.X = Direction * Settings.HoverSidePush;
				S.Opacity = Settings.HoverSideOpacity;
			}
			break;
		case EAugmentLayoutMode::Outro:
			if (i == SelectedIndex)
			{
				S.Scale = Settings.OutroSelectedScale;
				// 원래 자리에서 줄의 가운데 까지의 차이로 Basis가 없으면 제자리 확대함
				S.Translation = (Basis.bValid && Basis.SelectionTranslations.IsValidIndex(i))
					? Basis.SelectionTranslations[i]
					: FVector2D::ZeroVector;
			}
			else
			{
				S.Scale = Settings.OutroRejectedScale;
				// 선택카드에서 멀어짐
				S.Translation.X = Direction * Settings.OutroRejectedPush;
				S.Opacity = 0.f;
			}
			break;
			
		default:
			break; // Idle
		}
	}
}

void FAugmentCardLayoutController::Reset(int32 InNumCards, const FAugmentCardLayoutSettings& Settings)
{
	Mode = EAugmentLayoutMode::Idle;
	HoveredIndex = INDEX_NONE;
	SelectedIndex = INDEX_NONE;
	OutroElapsed = 0.f;
	RejectedProgress = 0.f;
	
	Basis = FAugmentLayoutBasis();
	
	CurrentStates.Reset();
	CurrentStates.SetNum(InNumCards);
	OutroStartStates = CurrentStates;
	
	ComputeTargets(Settings);
	CurrentStates = TargetStates;
}

void FAugmentCardLayoutController::SetBasis(const FAugmentLayoutBasis& InBasis,
	const FAugmentCardLayoutSettings& Settings)
{
	Basis = InBasis;
	
	// 목표를 다시 계산함 Idle일 때 갱신
	ComputeTargets(Settings);
}

bool FAugmentCardLayoutController::HasValidBasis() const
{
	return Basis.bValid;
}

void FAugmentCardLayoutController::SetHovered(int32 InIndex, const FAugmentCardLayoutSettings& Settings)
{
	if (Mode == EAugmentLayoutMode::Outro)
	{
		return;
	}
	
	HoveredIndex = InIndex;
	Mode = HoveredIndex == INDEX_NONE 
		? EAugmentLayoutMode::Idle 
		: EAugmentLayoutMode::Hover;
	
	ComputeTargets(Settings);
}

void FAugmentCardLayoutController::BeginOutro(int32 InSelectedIndex, const FAugmentCardLayoutSettings& Settings)
{
	SelectedIndex = InSelectedIndex;
	HoveredIndex = INDEX_NONE;
	
	Mode = EAugmentLayoutMode::Outro;
	OutroElapsed = 0.f;
	RejectedProgress = 0.f;
	
	// 현재 Hover 상태에서 그대로 Outro 시작
	OutroStartStates = CurrentStates;

	ComputeTargets(Settings);
}

bool FAugmentCardLayoutController::Tick(float DeltaTime, const FAugmentCardLayoutSettings& Settings)
{
	if (CurrentStates.IsEmpty())
	{
		return false;
	}
	
	if (Mode == EAugmentLayoutMode::Outro)
	{
		OutroElapsed += DeltaTime;
		// 실제 시간 진행률
		const float Raw = FMath::Clamp(
			OutroElapsed / FMath::Max(Settings.OutroDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
		
		// Raw에 연출 Curve를 적용한 진행률. 이동/스케일/디졸브 사용
		const float Alpha = Settings.OutroCurve
			? Settings.OutroCurve->GetFloatValue(Raw)
			: Raw;
		
		// 마지막 구간 Fade 전용 진행률
		const float FadeAlpha = (Settings.OutroRejectedFadeStart >= 1.f)
			? 0.f
			: FMath::Clamp((Raw - Settings.OutroRejectedFadeStart) 
				/ (1.f - Settings.OutroRejectedFadeStart), 0.f, 1.f);
		
		for (int32 i = 0; i < CurrentStates.Num(); ++i)
		{
			CurrentStates[i].Translation = FMath::Lerp(
				OutroStartStates[i].Translation, TargetStates[i].Translation, Alpha);
			CurrentStates[i].Scale = FMath::Lerp(
				OutroStartStates[i].Scale, TargetStates[i].Scale, Alpha);
			CurrentStates[i].Opacity = FMath::Lerp(
				OutroStartStates[i].Opacity, TargetStates[i].Opacity, FadeAlpha);
		}
		
		// 화면이 이 값을 카드에 넘긴다 
		RejectedProgress = FMath::Clamp(Alpha, 0.f, 1.f);
		
		if (Raw >= 1.f)
		{
			CurrentStates = TargetStates;
			RejectedProgress = 1.f;
			
			Mode = EAugmentLayoutMode::Idle;
			
			return true;
		}
		return false;
	}
	
	// Hover / Idle 
	for (int32 i = 0; i < CurrentStates.Num(); ++i)
	{
		CurrentStates[i].Translation = FMath::Vector2DInterpTo(
			CurrentStates[i].Translation, TargetStates[i].Translation,
			DeltaTime, Settings.HoverInterpSpeed);
		CurrentStates[i].Scale = FMath::FInterpTo(
			CurrentStates[i].Scale, TargetStates[i].Scale,
			DeltaTime, Settings.HoverInterpSpeed);
		CurrentStates[i].Opacity = FMath::FInterpTo(
			CurrentStates[i].Opacity, TargetStates[i].Opacity,
			DeltaTime, Settings.HoverInterpSpeed);
	}
	
	return false;
}

bool FAugmentCardLayoutController::IsOutroPlaying() const
{
	return Mode == EAugmentLayoutMode::Outro;
}

const TArray<FAugmentCardLayoutState>& FAugmentCardLayoutController::GetStates() const
{
	return CurrentStates;
}

float FAugmentCardLayoutController::GetRejectedProgress() const
{
	return RejectedProgress;
}

int32 FAugmentCardLayoutController::GetHoveredIndex() const
{
	return HoveredIndex;
}

int32 FAugmentCardLayoutController::GetSelectedIndex() const
{
	return SelectedIndex;
}

int32 FAugmentCardLayoutController::GetNumCards() const
{
	return CurrentStates.Num();
}

