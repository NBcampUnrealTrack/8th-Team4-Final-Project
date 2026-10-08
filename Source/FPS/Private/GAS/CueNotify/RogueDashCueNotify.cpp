#include "GAS/CueNotify/RogueDashCueNotify.h"

#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "Containers/Ticker.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Player/DominationPlayerState.h"
#include "Rendering/DrawElements.h"
#include "Tags/GameplayTags.h"
#include "Widgets/SLeafWidget.h"

namespace
{
	// Thin streaks at the screen edges that rush toward the centre and fade, for the dash's own view.
	// The centre stays clear for aiming.
	class SRogueDashSpeedLines : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SRogueDashSpeedLines) : _Duration(.35f), _LineCount(48), _Opacity(.35f) {}
			SLATE_ARGUMENT(float, Duration)
			SLATE_ARGUMENT(int32, LineCount)
			SLATE_ARGUMENT(float, Opacity)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Duration = FMath::Max(InArgs._Duration, .05f);
			Opacity = InArgs._Opacity;
			StartTime = FSlateApplication::Get().GetCurrentTime();
			FRandomStream Random(static_cast<int32>(FPlatformTime::Cycles()));
			for (int32 I = 0; I < InArgs._LineCount; ++I)
			{
				FLine& Line = Lines.AddDefaulted_GetRef();
				Line.Angle = (I + Random.FRandRange(-.4f, .4f)) * UE_TWO_PI / FMath::Max(InArgs._LineCount, 1);
				Line.Start = Random.FRandRange(.85f, 1.1f);
				Line.Length = Random.FRandRange(.12f, .3f);
				Line.Travel = Random.FRandRange(.25f, .4f);
				Line.Alpha = Random.FRandRange(.4f, 1.f);
				Line.Thickness = Random.FRandRange(1.f, 2.5f);
			}
			SetVisibility(EVisibility::HitTestInvisible);
			ForceVolatile(true);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
			FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
		{
			const float T = static_cast<float>((Args.GetCurrentTime() - StartTime) / Duration);
			if (T < 0.f || T >= 1.f)
			{
				return LayerId;
			}
			// Quick fade in, longer fade out; the streaks keep moving inward meanwhile.
			const float Envelope = FMath::SmoothStep(0.f, .12f, T) * (1.f - FMath::SmoothStep(.35f, 1.f, T));
			const float Move = 1.f - FMath::Square(1.f - T);
			const FVector2D Size = AllottedGeometry.GetLocalSize();
			const FVector2f Centre(Size.X * .5f, Size.Y * .5f);
			const float Radius = .5f * FMath::Sqrt(static_cast<float>(Size.X * Size.X + Size.Y * Size.Y));
			for (const FLine& Line : Lines)
			{
				const FVector2f Dir(FMath::Cos(Line.Angle), FMath::Sin(Line.Angle));
				const float Outer = (Line.Start - Line.Travel * Move) * Radius;
				const float Inner = FMath::Max(Outer - Line.Length * Radius, .45f * Radius);
				if (Outer <= Inner)
				{
					continue;
				}
				TArray<FVector2f> Points;
				Points.Add(Centre + Dir * Inner);
				Points.Add(Centre + Dir * Outer);
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), MoveTemp(Points),
					ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Opacity * Line.Alpha * Envelope), true, Line.Thickness);
			}
			return LayerId;
		}

	private:
		struct FLine
		{
			float Angle = 0.f;
			float Start = 1.f;
			float Length = .2f;
			float Travel = .3f;
			float Alpha = 1.f;
			float Thickness = 1.f;
		};
		TArray<FLine> Lines;
		double StartTime = 0.0;
		float Duration = .35f;
		float Opacity = .35f;
	};
}

void URogueDashShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(Duration);
	OutInfo.BlendIn = 0.f;
	OutInfo.BlendOut = 0.f;
}

void URogueDashShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	Elapsed = 0.f;
}

void URogueDashShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	Elapsed += Params.DeltaTime;
	const float KickIn = FMath::Clamp(KickInTime, .01f, Duration);
	const float Amount = Elapsed < KickIn
		? FMath::SmoothStep(0.f, KickIn, Elapsed)
		: 1.f - FMath::SmoothStep(KickIn, Duration, Elapsed);
	OutResult.FOV = FieldOfViewKick * Amount * Params.GetTotalScale();
}

bool URogueDashShakePattern::IsFinishedImpl() const
{
	return Elapsed >= Duration;
}

URogueDashCameraShake::URogueDashCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	SetRootShakePattern(CreateDefaultSubobject<URogueDashShakePattern>(TEXT("DashKick")));
}

bool URogueDashCueNotify::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	const bool bResult = Super::OnExecute_Implementation(Target, Parameters);
	if (bLocalSpeedFeedback)
	{
		PlayLocalSpeedFeedback(Cast<APawn>(Target));
	}
	return bResult;
}

void URogueDashCueNotify::PlayLocalSpeedFeedback(APawn* Pawn) const
{
	// Only the dashing player's own view: other players see the body animation.
	APlayerController* Controller = Pawn && Pawn->IsLocallyControlled() ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Controller || !Controller->IsLocalController() || Pawn->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (FieldOfViewKick > 0.f && Controller->PlayerCameraManager)
	{
		if (UCameraShakeBase* Shake = Controller->PlayerCameraManager->StartCameraShake(URogueDashCameraShake::StaticClass()))
		{
			if (auto* Pattern = Cast<URogueDashShakePattern>(Shake->GetRootShakePattern()))
			{
				Pattern->FieldOfViewKick = FieldOfViewKick;
				Pattern->Duration = FeedbackDuration;
			}
		}
	}

	ULocalPlayer* Player = Controller->GetLocalPlayer();
	UGameViewportClient* Viewport = Player ? Player->ViewportClient.Get() : nullptr;
	if (SpeedLineCount <= 0 || SpeedLineOpacity <= 0.f || !Viewport || !FSlateApplication::IsInitialized())
	{
		return;
	}
	const TSharedRef<SWidget> Lines = SNew(SRogueDashSpeedLines)
		.Duration(FeedbackDuration)
		.LineCount(SpeedLineCount)
		.Opacity(SpeedLineOpacity);
	Viewport->AddViewportWidgetForPlayer(Player, Lines, 10);

	// Remove the overlay once it has faded out.
	const double EndTime = FPlatformTime::Seconds() + FeedbackDuration + .05;
	TWeakObjectPtr<UGameViewportClient> WeakViewport(Viewport);
	TWeakObjectPtr<ULocalPlayer> WeakPlayer(Player);
	TWeakPtr<SWidget> WeakLines(Lines);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakViewport, WeakPlayer, WeakLines, EndTime](float)
	{
		if (FPlatformTime::Seconds() < EndTime)
		{
			return true;
		}
		const TSharedPtr<SWidget> Widget = WeakLines.Pin();
		if (Widget && WeakViewport.IsValid() && WeakPlayer.IsValid())
		{
			WeakViewport->RemoveViewportWidgetForPlayer(WeakPlayer.Get(), Widget.ToSharedRef());
		}
		return false;
	}));
}

UAnimMontage* URogueDashCueNotify::ResolveMontage3P(AActor* Target) const
{
	const auto* Character = Cast<AShooterCharacter>(Target);
	const auto* Combat = Character ? Character->GetCombatComponent() : nullptr;
	if (!Combat || !Combat->GetCurrentWeapon()) return Super::ResolveMontage3P(Target);

	const auto* State = Character->GetPlayerState<ADominationPlayerState>();
	EDominationCombatClass Class = State ? State->GetCombatClass() : EDominationCombatClass::Unselected;
	// Cosmetic mesh replacement and late PlayerState replication must not select a different grip.
	if (Class == EDominationCombatClass::Unselected)
		for (const auto& Definition : GetDefault<UDominationClassSettings>()->CombatClasses)
			if (Definition.PawnClass.Get() && Character->IsA(Definition.PawnClass.Get()))
			{
				Class = Definition.CombatClass;
				break;
			}

	const FDashClassMontages* Fitted = ClassMontages.Find(Class);
	if (!Fitted) return Super::ResolveMontage3P(Target);
	const FGameplayTag Type = Combat->GetCurrentWeaponTag();
	UAnimMontage* Montage = nullptr;
	if (Type.MatchesTag(RogueTags::Weapon_Type_Knife)) Montage = Fitted->Knife;
	else if (Type.MatchesTag(RogueTags::Weapon_Type_Pistol)) Montage = Fitted->Pistol;
	else if (const auto* Definition = GetDefault<UDominationClassSettings>()->FindDefinition(Class);
		Definition && Type.MatchesTag(Definition->PrimaryWeaponTag)) Montage = Fitted->Primary;
	return Montage ? Montage : Super::ResolveMontage3P(Target);
}
