#include "GAS/CueNotify/RogueDashCueNotify.h"

#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Customization/AppearanceBoneDependencyComponent.h"
#include "Containers/Ticker.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/DominationPlayerState.h"
#include "Rendering/DrawElements.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"
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

	struct FDashAfterimage
	{
		TArray<TWeakObjectPtr<UPoseableMeshComponent>> Parts;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		double Born = 0.0;
	};

	struct FDashAfterimageTrail
	{
		TWeakObjectPtr<AActor> Target;
		TWeakObjectPtr<UMaterialInterface> Material;
		TArray<FDashAfterimage> Images;
		int32 Remaining = 0;
		int32 Total = 0;
		double Next = 0.0;
		double Interval = .04;
		double Lifetime = .3;
		FName OpacityParameter;
		float Opacity = 1.f;
		float BaseOpacity = 1.f;
		bool bHaveBaseOpacity = false;
		FName ColorParameter;
		FLinearColor FarColor = FLinearColor::Green;
		FLinearColor NearColor = FLinearColor::Blue;
		int32 ForcedLOD = 0;
	};

	// Freezes the target's currently visible third-person meshes (body parts and held weapon) into translucent copies.
	void SpawnAfterimage(FDashAfterimageTrail& Trail, double Now)
	{
		AActor* Target = Trail.Target.Get();
		UMaterialInterface* Material = Trail.Material.Get();
		if (!Target || !Material)
		{
			return;
		}

		TArray<USkeletalMeshComponent*> Sources;
		Target->GetComponents(Sources);
		if (const auto* Shooter = Cast<AShooterCharacter>(Target))
		{
			const auto* Combat = Shooter->GetCombatComponent();
			if (AWeaponBase* Weapon = Combat ? Combat->GetCurrentWeapon() : nullptr)
			{
				Sources.Add(Weapon->GetMesh3P());
			}
		}

		FDashAfterimage Image;
		Image.Born = Now;
		UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material, Target);
		if (!Dynamic)
		{
			return;
		}
		if (!Trail.bHaveBaseOpacity)
		{
			// The material's own opacity, faded from here.
			Trail.BaseOpacity = Dynamic->K2_GetScalarParameterValue(Trail.OpacityParameter);
			Trail.bHaveBaseOpacity = true;
		}
		Dynamic->SetScalarParameterValue(Trail.OpacityParameter, Trail.BaseOpacity * Trail.Opacity);
		if (!Trail.ColorParameter.IsNone())
		{
			// Oldest copy gets FarColor, the newest NearColor.
			const int32 Index = Trail.Total - Trail.Remaining;
			const float Along = Trail.Total > 1 ? static_cast<float>(Index) / (Trail.Total - 1) : 1.f;
			Dynamic->SetVectorParameterValue(Trail.ColorParameter, FMath::Lerp(Trail.FarColor, Trail.NearColor, Along));
		}
		Image.Material = Dynamic;

		for (USkeletalMeshComponent* Source : Sources)
		{
			// Third-person visuals only: skip the first-person arms, hidden gameplay drivers and helper meshes.
			if (!Source || !Source->GetSkeletalMeshAsset() || !Source->IsRegistered() || !Source->IsVisible() || Source->bHiddenInGame
				|| Source->bOnlyOwnerSee || !Source->bRenderInMainPass || Source->IsA<UAppearanceBoneDependencyComponent>())
			{
				continue;
			}
			auto* Part = NewObject<UPoseableMeshComponent>(Target, NAME_None, RF_Transient);
			Part->SetSkinnedAssetAndUpdate(Source->GetSkeletalMeshAsset());
			Part->SetWorldTransform(Source->GetComponentTransform());
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetGenerateOverlapEvents(false);
			Part->SetCanEverAffectNavigation(false);
			Part->SetCastShadow(false);
			Part->bReceivesDecals = false;
			// The dashing player never sees their own afterimages.
			Part->SetOwnerNoSee(true);
			Part->SetForcedLOD(Trail.ForcedLOD);
			Part->RegisterComponent();
			// A follower of a leader pose has no pose of its own: copy the leader's.
			USkeletalMeshComponent* Pose = Cast<USkeletalMeshComponent>(Source->LeaderPoseComponent.Get());
			if (!Pose)
			{
				Pose = Source;
			}
			Part->CopyPoseFromSkeletalComponent(Pose);
			for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
			{
				Part->SetMaterial(Slot, Dynamic);
			}
			Image.Parts.Add(Part);
		}
		if (!Image.Parts.IsEmpty())
		{
			Trail.Images.Add(MoveTemp(Image));
		}
	}

	void DestroyAfterimage(FDashAfterimage& Image)
	{
		for (const TWeakObjectPtr<UPoseableMeshComponent>& Part : Image.Parts)
		{
			if (UPoseableMeshComponent* Mesh = Part.Get())
			{
				Mesh->DestroyComponent();
			}
		}
		Image.Parts.Reset();
	}

	bool TickAfterimageTrail(FDashAfterimageTrail& Trail)
	{
		const double Now = FPlatformTime::Seconds();
		if (!Trail.Target.IsValid())
		{
			for (FDashAfterimage& Image : Trail.Images)
			{
				DestroyAfterimage(Image);
			}
			return false;
		}
		while (Trail.Remaining > 0 && Now >= Trail.Next)
		{
			SpawnAfterimage(Trail, Now);
			--Trail.Remaining;
			Trail.Next += Trail.Interval;
		}
		for (int32 I = Trail.Images.Num() - 1; I >= 0; --I)
		{
			FDashAfterimage& Image = Trail.Images[I];
			const float Age = static_cast<float>((Now - Image.Born) / Trail.Lifetime);
			if (Age >= 1.f)
			{
				DestroyAfterimage(Image);
				Trail.Images.RemoveAt(I);
				continue;
			}
			if (UMaterialInstanceDynamic* Dynamic = Image.Material.Get())
			{
				Dynamic->SetScalarParameterValue(Trail.OpacityParameter, Trail.BaseOpacity * Trail.Opacity * FMath::Square(1.f - Age));
			}
		}
		return Trail.Remaining > 0 || !Trail.Images.IsEmpty();
	}
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
	if (bAfterimages)
	{
		PlayAfterimages(Target);
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

void URogueDashCueNotify::PlayAfterimages(AActor* Target) const
{
	// Other players' views only: the dashing player sees the first-person feedback instead.
	const auto* Pawn = Cast<APawn>(Target);
	if (!Pawn || Pawn->IsLocallyControlled() || AfterimageCount <= 0 || Pawn->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UMaterialInterface* Material = AfterimageMaterial.LoadSynchronous();
	if (!Material)
	{
		return;
	}

	const TSharedRef<FDashAfterimageTrail> Trail = MakeShared<FDashAfterimageTrail>();
	Trail->Target = Target;
	Trail->Material = Material;
	Trail->Remaining = AfterimageCount;
	Trail->Total = AfterimageCount;
	Trail->ColorParameter = AfterimageColorParameter;
	Trail->FarColor = AfterimageFarColor;
	Trail->NearColor = AfterimageNearColor;
	Trail->ForcedLOD = AfterimageForcedLOD;
	Trail->Next = FPlatformTime::Seconds();
	Trail->Interval = AfterimageInterval;
	Trail->Lifetime = AfterimageLifetime;
	Trail->OpacityParameter = AfterimageOpacityParameter;
	Trail->Opacity = AfterimageOpacity;
	if (!TickAfterimageTrail(*Trail))
	{
		return;
	}
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Trail](float)
	{
		return TickAfterimageTrail(*Trail);
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
