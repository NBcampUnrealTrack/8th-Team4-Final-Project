#include "UI/InGame/Domination/DominationClassPreviewStage.h"

#include "Animation/AnimMontage.h"
#include "Character/ShooterCharacter.h"
#include "Character/CharacterPresentationTags.h"
#include "Combat/CombatComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "ContentStreaming.h"
#include "Customization/AppearanceLocalPlayerSubsystem.h"
#include "Customization/GameplayAppearanceComponent.h"
#include "Customization/ReplicatedAppearance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"
#include "Weapon/WeaponBase.h"

namespace
{
	// Mutable 비동기 생성 완료를 0.1초 간격으로 최대 15초까지 기다린다.
	constexpr float AppearancePollInterval = 0.1f;
	constexpr int32 AppearanceMaxPolls = 150;

	// 스트리밍 뷰 갱신 주기. 등록 유지 시간을 주기보다 길게 잡아 공백이 없게 한다.
	constexpr float StreamingViewInterval = 1.0f;
	constexpr float StreamingViewDuration = 1.5f;

	FName ToAppearanceClassId(const EDominationCombatClass CombatClass)
	{
		switch (CombatClass)
		{
		case EDominationCombatClass::Ranger: return TEXT("Ranger");
		case EDominationCombatClass::Trooper: return TEXT("Trooper");
		case EDominationCombatClass::Vanguard: return TEXT("Vanguard");
		default: return NAME_None;
		}
	}

	// 레벨 조명이 프리뷰에 새지 않도록 Channel 1만 남긴다.
	void ApplyPreviewLightingChannels(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return;
		}

		TArray<UPrimitiveComponent*> Primitives;
		Actor->GetComponents<UPrimitiveComponent>(Primitives);

		for (UPrimitiveComponent* Primitive : Primitives)
		{
			Primitive->SetLightingChannels(false, true, false);
		}
	}

	void ForcePreviewAnimRefresh(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return;
		}

		TArray<USkinnedMeshComponent*> Meshes;
		Actor->GetComponents<USkinnedMeshComponent>(Meshes);

		for (USkinnedMeshComponent* Mesh : Meshes)
		{
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		}
	}

	// bAutoActive 다시 키기
	void RestartEffect(UNiagaraComponent* Effect)
	{
		if (!IsValid(Effect))
		{
			return;
		}

		Effect->Deactivate();
		Effect->Activate(/*bReset=*/true);
	}

	void CollectPawnWeapons(AShooterCharacter* Pawn, TArray<AWeaponBase*>& OutWeapons)
	{
		UCombatComponent* Combat = IsValid(Pawn) ? Pawn->GetCombatComponent() : nullptr;
		if (!IsValid(Combat))
		{
			return;
		}

		// 화면에 보이지 않는 무기도 복제 차단과 정리를 위해 모두 수집
		static const EWeaponSlot PreviewSlots[] = {
			EWeaponSlot::Primary,
			EWeaponSlot::Secondary,
			EWeaponSlot::Melee };

		for (const EWeaponSlot Slot : PreviewSlots)
		{
			if (AWeaponBase* Weapon = Combat->GetWeaponInSlot(Slot))
			{
				OutWeapons.AddUnique(Weapon);
			}
		}

		// CurrentWeapon이 세 슬롯 어디에도 없는 경우
		if (AWeaponBase* CurrentWeapon = Combat->GetCurrentWeapon())
		{
			OutWeapons.AddUnique(CurrentWeapon);
		}
	}
}

ADominationClassPreviewStage::ADominationClassPreviewStage()
{
	PrimaryActorTick.bCanEverTick = false;

	//  프리뷰 Pawn은 부착하지 않고 따로 스폰
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	SceneCapture->SetupAttachment(SceneRoot);

	ChangeEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ChangeEffect"));
	ChangeEffect->SetupAttachment(SceneRoot);
	ChangeEffect->bAutoActivate = false;

	LockInEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LockInEffect"));
	LockInEffect->SetupAttachment(SceneRoot);
	LockInEffect->bAutoActivate = false;
}

void ADominationClassPreviewStage::BeginPlay()
{
	Super::BeginPlay();

	// 클라이언트마다 프리뷰 화면이 섞이지 않도록 RenderTarget을 따로 만듦
	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->RenderTargetFormat = RenderTargetFormat;
	RenderTarget->InitAutoFormat(
		FMath::Max(1, RenderTargetSize.X),
		FMath::Max(1, RenderTargetSize.Y));
	SceneCapture->TextureTarget = RenderTarget;

	if (IsValid(PreviewMaterialTemplate))
	{
		PreviewMID = UMaterialInstanceDynamic::Create(PreviewMaterialTemplate, this);
		PreviewMID->SetTextureParameterValue(TEXT("PreviewRT"), RenderTarget);
	}

	// ShowOnly에 등록한 프리뷰 오브젝트들만 캡처
	SceneCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	// RGB에는 SceneColor, A에는 반전된 Opacity를 캡처한다.
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	// 끈 경우 RegisterPreviewComponents 에서 한 번씩 직접 캡처한다.
	SceneCapture->bCaptureEveryFrame = bCaptureEveryFrame;
	SceneCapture->bCaptureOnMovement = false;

	// 클래스 변경/확정 VFX도 프리뷰 캡처 대상에 포함한다.
	SceneCapture->ShowOnlyComponent(ChangeEffect);
	SceneCapture->ShowOnlyComponent(LockInEffect);

	// 카드 전환 때마다 Mutable 생성을 기다리지 않도록 세 클래스를 미리 만들어 둔다.
	SpawnPreviewPawns();
	RegisterPreviewComponents();
	// Identity/cloud data can arrive after the selection screen has already opened.
	// Poll only while this stage exists; unchanged saves never trigger generation.
	GetWorldTimerManager().SetTimer(SavedAppearanceTimerHandle, this,
		&ThisClass::RefreshSavedAppearances, 1.0f, /*bLoop=*/true);

	KeepPreviewTexturesStreamed();
	GetWorldTimerManager().SetTimer(
		StreamingTimerHandle,
		this,
		&ThisClass::KeepPreviewTexturesStreamed,
		StreamingViewInterval,
		/*bLoop=*/true);
}

void ADominationClassPreviewStage::KeepPreviewTexturesStreamed()
{
	// SceneCapture는 스트리밍 뷰가 아니라 먼 스테이지의 텍스처가 최저 Mip에 머문다. 캡처 위치를 뷰로 알린다.
	IStreamingManager::Get().AddViewLocation(
		SceneCapture->GetComponentLocation(),
		/*BoostFactor=*/1.0f,
		/*bOverrideLocation=*/false,
		StreamingViewDuration);
}

void ADominationClassPreviewStage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyPreviewPawns();

	Super::EndPlay(EndPlayReason);
}

void ADominationClassPreviewStage::SetPreviewClass(const EDominationCombatClass CombatClass)
{
	if (CombatClass == CurrentPreviewClass)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(InspectTimerHandle);

	CurrentPreviewClass = CombatClass;
	PreviewPawn = PreviewPawns.FindRef(CombatClass);

	// 외형이 완성되기 전엔 캡처에서 빼 둔다. PollAppearanceApplied가 완성 시점에 띄운다.
	if (PendingAppearanceClasses.Contains(CombatClass))
	{
		RegisterPreviewComponents();
		return;
	}

	ShowCurrentPreview();
}

void ADominationClassPreviewStage::ShowCurrentPreview()
{
	RegisterPreviewComponents();

	PlayChangeEffect();
	// 첫 애님 업데이트에서 ShooterAnimInstance가 장착 몽타주로 덮어쓰므로 한 틱 뒤에 재생
	GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			ScheduleInspect(PlayPreviewMontage(DrawMontages));
		}));
}

void ADominationClassPreviewStage::SpawnPreviewPawns()
{
	const UDominationClassSettings* ClassSettings = GetDefault<UDominationClassSettings>();
	if (!IsValid(ClassSettings))
	{
		return;
	}

	for (const FDominationCombatClassDefinition& Definition : ClassSettings->CombatClasses)
	{
		AShooterCharacter* Pawn = SpawnPreviewPawn(Definition.CombatClass);
		if (!IsValid(Pawn))
		{
			continue;
		}

		PreviewPawns.Add(Definition.CombatClass, Pawn);
		if (RequestSavedAppearance(Definition.CombatClass, Pawn))
		{
			PendingAppearanceClasses.Add(Definition.CombatClass);
		}
	}

	if (!PendingAppearanceClasses.IsEmpty())
	{
		AppearancePollsRemaining = AppearanceMaxPolls;
		GetWorldTimerManager().SetTimer(
			AppearanceTimerHandle,
			this,
			&ThisClass::PollAppearanceApplied,
			AppearancePollInterval,
			/*bLoop=*/true);
	}
}

AShooterCharacter* ADominationClassPreviewStage::SpawnPreviewPawn(const EDominationCombatClass CombatClass)
{
	UWorld* World = GetWorld();
	const UDominationClassSettings* ClassSettings = GetDefault<UDominationClassSettings>();
	UClass* PawnClass = IsValid(ClassSettings)
		? ClassSettings->ResolvePawnClass(CombatClass)
		: nullptr;

	if (!IsValid(World) || PawnClass == nullptr)
	{
		return nullptr;
	}

	// 클래스 애니메이션에서 방향을 결정한다. 메시 교체를 보상하려고
	// Vanguard만 추가 회전하면 실제 게임과 선택창 자세가 다시 달라진다.
	const FTransform PreviewTransform = GetActorTransform();

	// 프리뷰 Pawn이 네트워크에 복제되기 전에 설정할 수 있도록 Deferred Spawn한다.
	AShooterCharacter* Pawn = World->SpawnActorDeferred<AShooterCharacter>(
		PawnClass,
		PreviewTransform,
		// Keep the presentation pawn owned by its stage for lifecycle/visibility.
		/*Owner=*/this,
		/*Instigator=*/nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Pawn))
	{
		return nullptr;
	}

	Pawn->SetReplicates(false);
	Pawn->SetActorEnableCollision(false);
	// 이동은 끈 채로, AnimGraph에는 서 있는 미리보기임을 전달한다.
	Pawn->Tags.AddUnique(CharacterPresentationTags::ClassSelectionPreview);

	// BeginPlay를 실행
	Pawn->FinishSpawning(PreviewTransform);

	// BeginPlay가 GravityScale에 배수를 곱하므로 그 뒤에 실행
	// 컴포넌트를 Deactivate하지는 않는다. AnimBP가 멈춰 T-포즈로 떨어짐
	if (UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement())
	{
		Movement->GravityScale = 0.0f;
		Movement->SetMovementMode(MOVE_None);
	}

	TArray<AWeaponBase*> Weapons;
	CollectPawnWeapons(Pawn, Weapons);

	// 무기는 Pawn의 bReplicates를 물려받지 않는 별개 액터다. 각자 끊어야 한다.
	for (AWeaponBase* Weapon : Weapons)
	{
		Weapon->SetReplicates(false);
		ApplyPreviewLightingChannels(Weapon);
		PreviewWeapons.AddUnique(Weapon);
	}

	ApplyPreviewLightingChannels(Pawn);
	ForcePreviewAnimRefresh(Pawn);

	return Pawn;
}

void ADominationClassPreviewStage::PlayChangeEffect()
{
	RestartEffect(ChangeEffect);
}

void ADominationClassPreviewStage::PlayLockInEffect()
{
	RestartEffect(LockInEffect);
}

float ADominationClassPreviewStage::PlayPreviewMontage(
	const TMap<EDominationCombatClass, TObjectPtr<UAnimMontage>>& Montages) const
{
	const TObjectPtr<UAnimMontage>* Montage = Montages.Find(CurrentPreviewClass);
	if (!Montage || !IsValid(*Montage) || !IsValid(PreviewPawn))
	{
		return 0.0f;
	}

	return PreviewPawn->PlayAnimMontage(*Montage);
}

void ADominationClassPreviewStage::ScheduleInspect(const float MontageDuration)
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !InspectMontages.Contains(CurrentPreviewClass))
	{
		return;
	}

	// 재생 중인 몽타주가 끝난 뒤부터 재생해야함
	const float Delay = MontageDuration
		+ FMath::FRandRange(InspectDelayRange.X, InspectDelayRange.Y);

	World->GetTimerManager().SetTimer(
		InspectTimerHandle,
		this,
		&ThisClass::PlayInspect,
		FMath::Max(Delay, 0.1f),
		/*bLoop=*/false);
}

void ADominationClassPreviewStage::PlayInspect()
{
	ScheduleInspect(PlayPreviewMontage(InspectMontages));
}

bool ADominationClassPreviewStage::RequestSavedAppearance(
	const EDominationCombatClass CombatClass, AShooterCharacter* Pawn)
{
	// 프리뷰 Pawn은 PlayerState가 없어 스스로 외형을 읽지 못해 로컬 저장본을 직접 넣음
	const APlayerController* OwningPlayer = Cast<APlayerController>(GetOwner());
	ULocalPlayer* LocalPlayer = IsValid(OwningPlayer) ? OwningPlayer->GetLocalPlayer() : nullptr;
	UAppearanceLocalPlayerSubsystem* Storage = LocalPlayer
		? LocalPlayer->GetSubsystem<UAppearanceLocalPlayerSubsystem>()
		: nullptr;
	
	UGameplayAppearanceComponent* Appearance = Pawn->FindComponentByClass<UGameplayAppearanceComponent>();
	if (!IsValid(Appearance))
	{
		return false;
	}

	FReplicatedAppearance Data;
	Data.ClassId = ToAppearanceClassId(CombatClass);
	if (Data.ClassId.IsNone())
	{
		return false;
	}
	if (!Storage || Storage->LoadSavedAppearance(Data.ClassId, Data.Appearance) != ECustomizationLoadResult::Loaded)
		Data.Appearance = FCustomizationAppearance::DefaultForClass(Data.ClassId.ToString());
	Data.Revision = 1;
	if (const auto* Previous = RequestedAppearances.Find(CombatClass))
	{
		if (FCustomizationAppearance::StaticStruct()->CompareScriptStruct(Previous, &Data.Appearance, 0))
			return false;
	}
	RequestedAppearances.Add(CombatClass, Data.Appearance);

	Appearance->RequestAppearance(Data);
	// 카탈로그 누락 등은 동기로 실패한다. 기다리지 않고 기본 외형으로 보여준다.
	return Appearance->GetFailure().IsEmpty();
}

void ADominationClassPreviewStage::RefreshSavedAppearances()
{
	bool bChanged = false;
	bool bCurrentChanged = false;
	for (const auto& Pair : PreviewPawns)
	{
		if (IsValid(Pair.Value) && RequestSavedAppearance(Pair.Key, Pair.Value))
		{
			PendingAppearanceClasses.Add(Pair.Key);
			bChanged = true;
			bCurrentChanged |= Pair.Key == CurrentPreviewClass;
		}
	}
	if (!bChanged) return;
	AppearancePollsRemaining = AppearanceMaxPolls;
	GetWorldTimerManager().SetTimer(AppearanceTimerHandle, this,
		&ThisClass::PollAppearanceApplied, AppearancePollInterval, /*bLoop=*/true);
	if (bCurrentChanged)
	{
		GetWorldTimerManager().ClearTimer(InspectTimerHandle);
		RegisterPreviewComponents();
	}
}

void ADominationClassPreviewStage::PollAppearanceApplied()
{
	const bool bTimedOut = --AppearancePollsRemaining <= 0;
	bool bCurrentFinished = false;

	for (auto It = PendingAppearanceClasses.CreateIterator(); It; ++It)
	{
		AShooterCharacter* Pawn = PreviewPawns.FindRef(*It);
		const UGameplayAppearanceComponent* Appearance = IsValid(Pawn)
			? Pawn->FindComponentByClass<UGameplayAppearanceComponent>()
			: nullptr;
		const bool bApplied = IsValid(Appearance) && Appearance->IsLatestAppearanceApplied();
		const bool bFailed = !IsValid(Appearance) || !Appearance->GetFailure().IsEmpty();
		if (!bApplied && !bFailed && !bTimedOut)
		{
			continue;
		}

		if (bApplied)
		{
			// 생성된 메시가 새로 붙었다. 조명 채널을 다시 건다.
			ApplyPreviewLightingChannels(Pawn);
		}
		else if (bFailed && IsValid(Appearance))
		{
			UE_LOG(LogTemp, Warning, TEXT("[ClassPreview] Appearance failed: %s"), *Appearance->GetFailure());
		}

		bCurrentFinished |= *It == CurrentPreviewClass;
		It.RemoveCurrent();
	}

	if (PendingAppearanceClasses.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(AppearanceTimerHandle);
	}

	if (bCurrentFinished)
	{
		ShowCurrentPreview();
	}
}

void ADominationClassPreviewStage::RegisterPreviewComponents()
{
	SceneCapture->ClearShowOnlyComponents();
	// ShowOnly filters camera visibility, not shadow casters. All cached pawns
	// occupy the same stage, so explicitly hide non-selected actors and weapons.
	// Actor hiding also covers visuals added by asynchronous appearance updates.
	for (const auto& Pair : PreviewPawns)
	{
		AShooterCharacter* Pawn = Pair.Value;
		if (!IsValid(Pawn)) continue;
		const bool bPreviewHidden = Pawn != PreviewPawn || PendingAppearanceClasses.Contains(Pair.Key);
		Pawn->SetActorHiddenInGame(bPreviewHidden);
		TArray<AWeaponBase*> Weapons;
		CollectPawnWeapons(Pawn, Weapons);
		const UCombatComponent* Combat = Pawn->GetCombatComponent();
		for (AWeaponBase* Weapon : Weapons)
		{
			// An attachment refresh while hidden also hides Mesh3P. Restore the
			// selected weapon's full presentation, not only its actor flag.
			const bool bShowWeapon = !bPreviewHidden && Combat && Weapon == Combat->GetCurrentWeapon();
			Weapon->SetPresentationActive(bShowWeapon);
		}
	}

	// 다른 클래스 Pawn과 외형 생성 중인 Pawn은 ShowOnly에 넣지 않아 캡처되지 않는다.
	if (IsValid(PreviewPawn) && !PendingAppearanceClasses.Contains(CurrentPreviewClass))
	{
		SceneCapture->ShowOnlyActorComponents(PreviewPawn);
		ApplyPreviewLightingChannels(PreviewPawn);
		ForcePreviewAnimRefresh(PreviewPawn);

		TArray<AWeaponBase*> Weapons;
		CollectPawnWeapons(PreviewPawn, Weapons);
		for (AWeaponBase* Weapon : Weapons)
		{
			SceneCapture->ShowOnlyActorComponents(Weapon);
			ApplyPreviewLightingChannels(Weapon);
		}
	}

	// 두 VFX는 스테이지 소유라 Clear 뒤에 매번 다시 넣어야 한다.
	SceneCapture->ShowOnlyComponent(ChangeEffect);
	SceneCapture->ShowOnlyComponent(LockInEffect);

	if (!bCaptureEveryFrame)
	{
		SceneCapture->CaptureScene();
	}
}

void ADominationClassPreviewStage::DestroyPreviewPawns()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InspectTimerHandle);
		World->GetTimerManager().ClearTimer(AppearanceTimerHandle);
		World->GetTimerManager().ClearTimer(SavedAppearanceTimerHandle);
	}

	for (const TWeakObjectPtr<AWeaponBase>& WeaponPtr : PreviewWeapons)
	{
		if (AWeaponBase* Weapon = WeaponPtr.Get())
		{
			Weapon->Destroy();
		}
	}
	PreviewWeapons.Reset();

	for (const TPair<EDominationCombatClass, TObjectPtr<AShooterCharacter>>& Pair : PreviewPawns)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Destroy();
		}
	}
	PreviewPawns.Reset();
	PreviewPawn = nullptr;
	PendingAppearanceClasses.Reset();
	RequestedAppearances.Reset();
}

UMaterialInstanceDynamic* ADominationClassPreviewStage::GetPreviewMaterial() const
{
	return PreviewMID;
}
