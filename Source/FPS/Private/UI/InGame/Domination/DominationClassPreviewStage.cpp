#include "UI/InGame/Domination/DominationClassPreviewStage.h"

#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "Weapon/WeaponBase.h"

namespace
{
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
}

ADominationClassPreviewStage::ADominationClassPreviewStage()
{
	PrimaryActorTick.bCanEverTick = false;

	// 스폰 위치만 잡는 빈 루트. 프리뷰 Pawn은 부착하지 않고 따로 스폰한다.
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	SceneCapture->SetupAttachment(SceneRoot);

	ChangeEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ChangeEffect"));
	ChangeEffect->SetupAttachment(SceneRoot);
	// 기본값 true를 끄지 않으면 스테이지가 스폰되는 순간 두 VFX가 한 번씩 터짐
	ChangeEffect->bAutoActivate = false;

	LockInEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LockInEffect"));
	LockInEffect->SetupAttachment(SceneRoot);
	LockInEffect->bAutoActivate = false;
}

void ADominationClassPreviewStage::BeginPlay()
{
	Super::BeginPlay();

	// 클라이언트마다 프리뷰 화면이 섞이지 않도록 RenderTarget을 따로 만든다.
	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->RenderTargetFormat = RTF_RGBA16f;
	RenderTarget->InitAutoFormat(256, 512);
	SceneCapture->TextureTarget = RenderTarget;

	if (IsValid(PreviewMaterialTemplate))
	{
		PreviewMID = UMaterialInstanceDynamic::Create(PreviewMaterialTemplate, this);
		PreviewMID->SetTextureParameterValue(TEXT("PreviewRT"), RenderTarget);
	}

	// ShowOnly에 등록한 프리뷰 오브젝트만 캡처한다.
	SceneCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	// RGB에는 SceneColor, A에는 반전된 Opacity를 캡처한다.
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	// 매 프레임 갱신하므로 이동 시 별도 캡처는 사용하지 않는다.
	SceneCapture->bCaptureEveryFrame = true;
	SceneCapture->bCaptureOnMovement = false;

	// 클래스 변경/확정 VFX도 프리뷰 캡처 대상에 포함한다.
	SceneCapture->ShowOnlyComponent(ChangeEffect);
	SceneCapture->ShowOnlyComponent(LockInEffect);
}

void ADominationClassPreviewStage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyPreviewPawn();

	Super::EndPlay(EndPlayReason);
}

void ADominationClassPreviewStage::SetPreviewClass(const EDominationCombatClass CombatClass)
{
	if (CombatClass == CurrentPreviewClass)
	{
		return;
	}

	DestroyPreviewPawn();

	// 이전 Pawn을 이미 지웠으므로 중간에 실패해도 옛 클래스로 되돌아가지 않는다.
	// Unselected로 두어야 같은 클래스를 다시 요청했을 때 재시도된다.
	CurrentPreviewClass = EDominationCombatClass::Unselected;

	UWorld* World = GetWorld();
	const UDominationClassSettings* ClassSettings = GetDefault<UDominationClassSettings>();
	UClass* PawnClass = IsValid(ClassSettings)
		? ClassSettings->ResolvePawnClass(CombatClass)
		: nullptr;

	if (!IsValid(World) || PawnClass == nullptr)
	{
		return;
	}

	const FTransform StageTransform = GetActorTransform();

	// 프리뷰 Pawn이 네트워크에 복제되기 전에 설정할 수 있도록 Deferred Spawn한다.
	PreviewPawn = World->SpawnActorDeferred<AShooterCharacter>(
		PawnClass,
		StageTransform,
		/*Owner=*/nullptr,
		/*Instigator=*/nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(PreviewPawn))
	{
		return;
	}

	PreviewPawn->SetReplicates(false);
	PreviewPawn->SetActorEnableCollision(false);

	// BeginPlay를 실행
	PreviewPawn->FinishSpawning(StageTransform);

	// BeginPlay가 GravityScale에 배수를 곱하므로 그 뒤에 실행
	// 컴포넌트를 Deactivate하지는 않는다. AnimBP가 멈춰 T-포즈로 떨어짐
	if (UCharacterMovementComponent* Movement = PreviewPawn->GetCharacterMovement())
	{
		Movement->GravityScale = 0.0f;
		Movement->SetMovementMode(MOVE_None);
	}

	CollectPreviewWeapons();

	// 무기는 Pawn의 bReplicates를 물려받지 않는 별개 액터다. 각자 끊어야 한다.
	for (const TWeakObjectPtr<AWeaponBase>& WeaponPtr : PreviewWeapons)
	{
		if (AWeaponBase* Weapon = WeaponPtr.Get())
		{
			Weapon->SetReplicates(false);
		}
	}

	RegisterPreviewComponents();

	CurrentPreviewClass = CombatClass;

	PlayChangeEffect();
}

void ADominationClassPreviewStage::PlayChangeEffect()
{
	RestartEffect(ChangeEffect);
}

void ADominationClassPreviewStage::PlayLockInEffect()
{
	RestartEffect(LockInEffect);
}

void ADominationClassPreviewStage::CollectPreviewWeapons()
{
	PreviewWeapons.Reset();

	if (!IsValid(PreviewPawn))
	{
		return;
	}

	UCombatComponent* Combat = PreviewPawn->GetCombatComponent();
	if (!IsValid(Combat))
	{
		return;
	}

	// 화면에 보이지 않는 무기도 복제 차단과 정리를 위해 모두 수집한다.
	static const EWeaponSlot PreviewSlots[] = {
		EWeaponSlot::Primary,
		EWeaponSlot::Secondary,
		EWeaponSlot::Melee };

	for (const EWeaponSlot Slot : PreviewSlots)
	{
		if (AWeaponBase* Weapon = Combat->GetWeaponInSlot(Slot))
		{
			PreviewWeapons.AddUnique(Weapon);
		}
	}

	// CurrentWeapon이 세 슬롯 어디에도 없는 경우
	if (AWeaponBase* CurrentWeapon = Combat->GetCurrentWeapon())
	{
		PreviewWeapons.AddUnique(CurrentWeapon);
	}
}

void ADominationClassPreviewStage::RegisterPreviewComponents()
{
	SceneCapture->ClearShowOnlyComponents();

	if (IsValid(PreviewPawn))
	{
		SceneCapture->ShowOnlyActorComponents(PreviewPawn);
		ApplyPreviewLightingChannels(PreviewPawn);
	}

	for (const TWeakObjectPtr<AWeaponBase>& WeaponPtr : PreviewWeapons)
	{
		if (AWeaponBase* Weapon = WeaponPtr.Get())
		{
			SceneCapture->ShowOnlyActorComponents(Weapon);
			ApplyPreviewLightingChannels(Weapon);
		}
	}

	// 두 VFX는 스테이지 소유라 Clear 뒤에 매번 다시 넣어야 한다.
	SceneCapture->ShowOnlyComponent(ChangeEffect);
	SceneCapture->ShowOnlyComponent(LockInEffect);
}

void ADominationClassPreviewStage::DestroyPreviewPawn()
{
	for (const TWeakObjectPtr<AWeaponBase>& WeaponPtr : PreviewWeapons)
	{
		if (AWeaponBase* Weapon = WeaponPtr.Get())
		{
			Weapon->Destroy();
		}
	}
	PreviewWeapons.Reset();

	if (IsValid(PreviewPawn))
	{
		PreviewPawn->Destroy();
	}
	PreviewPawn = nullptr;
}

UMaterialInstanceDynamic* ADominationClassPreviewStage::GetPreviewMaterial() const
{
	return PreviewMID;
}