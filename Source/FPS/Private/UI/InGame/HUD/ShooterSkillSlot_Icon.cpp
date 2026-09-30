// ShooterSkillSlot_Icon.cpp
#include "UI/InGame/HUD/ShooterSkillSlot.h"

#include "Augment/AugmentDefinition.h"
#include "Components/HorizontalBox.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/Common/ShooterGauge.h"

void UShooterSkillSlot::ResetIconTexture()
{
	if (UMaterialInstanceDynamic* MID = GetIconMID())
	{
		MID->SetTextureParameterValue(TEXT("IconTexture"),DefaultIconTexture);
	}
}

void UShooterSkillSlot::RefreshSlotIcon()
{
	if (bUseDebugDriver)
	{
		return;
	}

	URogueAbilitySystemComponent* ASC = BoundASC.Get();
	if (!ASC || !CachedAbilitySpecHandle.IsValid())
	{
		return;
	}

	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(CachedAbilitySpecHandle);
	if (!Spec)
	{
		return;
	}

	const UAugmentDefinition* Definition = Cast<UAugmentDefinition>(Spec->SourceObject.Get());
	if (!Definition)
	{
		// 처리 완료로 캐시하면 다시 볼 수없으므로 그냥 return
		return;
	}

	const FPrimaryAssetId EntryId = Definition->GetPrimaryAssetId();

	if (EntryId == CachedIconDefinitionId || EntryId == PendingIconDefinitionId)
	{
		return;
	}

	// Visibility 활성화
	ApplyOccupiedState();

	if (Definition->Icon.IsNull())
	{
		ClearSlotIcon(EntryId);
		ApplyOccupiedState();
		return;
	}

	BeginIconLoad(EntryId, Definition->Icon.ToSoftObjectPath());
}

void UShooterSkillSlot::BeginIconLoad(const FPrimaryAssetId& RequestedId, const FSoftObjectPath& IconPath)
{
	ClearIconLoad();
	// 로드 시간 고려해서 리셋
	ResetIconDisplay();

	// Cached를 무효로 만든 뒤 Pending을 채워넣어야한다.
	PendingIconDefinitionId = RequestedId;

	FStreamableManager& Streamable = UAssetManager::Get().GetStreamableManager();

	IconLoadHandle = Streamable.RequestAsyncLoad(
		IconPath,
		FStreamableDelegate::CreateWeakLambda(this, [this, RequestedId, IconPath]()
		{
			// 로드 중 슬롯이 변경됐을 수 있으므로 이전 요청의 완료 콜백은 무시한다.
			if (RequestedId != PendingIconDefinitionId)
			{
				return;
			}

			UTexture2D* Loaded = Cast<UTexture2D>(IconPath.ResolveObject());
			if (Loaded == nullptr)
			{
				UE_LOG(LogTemp, Error, TEXT("[SkillSlot] 아이콘 로드 실패 %s"), *IconPath.ToString());
				ClearSlotIcon(RequestedId);
				ApplyOccupiedState();
				return;
			}

			CachedSlotIcon = Loaded;
			CachedIconDefinitionId = RequestedId;
			PendingIconDefinitionId = FPrimaryAssetId();

			IconLoadHandle.Reset();

			ApplyOccupiedState();
			SetIconTexture(CachedSlotIcon);
		}));
}

void UShooterSkillSlot::ClearIconLoad()
{
	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}
	PendingIconDefinitionId = FPrimaryAssetId();
}

void UShooterSkillSlot::ResetIconDisplay()
{
	CachedSlotIcon = nullptr;
	CachedIconDefinitionId = FPrimaryAssetId();
	ResetIconTexture();
}

void UShooterSkillSlot::ClearSlotIcon(const FPrimaryAssetId& ProcessedId)
{
	ClearIconLoad();
	ResetIconDisplay();

	CachedIconDefinitionId = ProcessedId;
}

void UShooterSkillSlot::ApplyOccupiedState()
{
	if (Image_Icon)
	{
		Image_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (Gauge_Cooldown)
	{
		Gauge_Cooldown->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UShooterSkillSlot::ApplyEmptySlotState()
{
	ClearSlotIcon(FPrimaryAssetId());

	if (Image_Icon)
	{
		Image_Icon->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Gauge_Cooldown)
	{
		Gauge_Cooldown->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Text_Cooldown)
	{
		Text_Cooldown->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (HBox_ChargeBars)
	{
		HBox_ChargeBars->SetVisibility(ESlateVisibility::Collapsed);
	}

	RecoveryEndWorldTime = 0.f;
	DisplayCooldownDuration = 0.f;
	LastDisplayedSeconds = INDEX_NONE;
	LastSpentCharges = INDEX_NONE;

	// 이전 스킬의 쿨다운 태그 구독 해제
	if (URogueAbilitySystemComponent* ASC = BoundASC.Get())
	{
		if (CachedCooldownTag.IsValid() && CooldownTagEventHandle.IsValid())
		{
			ASC->UnregisterGameplayTagEvent(
				CooldownTagEventHandle,
				CachedCooldownTag,
				EGameplayTagEventType::AnyCountChange);
		}
	}

	CooldownTagEventHandle.Reset();
	CachedCooldownTag = FGameplayTag();
	CachedAbilitySpecHandle = FGameplayAbilitySpecHandle();
	
	// 캐시만 Unknown으로 되돌린다
	ApplyAvailabilityVisual(false, false);
	LastAvailabilityVisual = EAvailabilityVisual::Unknown;
	bFullChargeFinalizePending = false;
}

void UShooterSkillSlot::ReleaseIconResources()
{
	ClearIconLoad();
}

bool UShooterSkillSlot::IsIconResolutionSettled() const
{
	if (bUseDebugDriver)
	{
		return true;
	}
	if (PendingIconDefinitionId.IsValid())
	{
		return true;
	}

	return CachedIconDefinitionId.IsValid();
}


UMaterialInstanceDynamic* UShooterSkillSlot::GetDigitMID()
{
	if (CachedDigitMID)
	{
		return CachedDigitMID;
	}
	if (!Text_Cooldown)
	{
		return nullptr;
	}

	FSlateFontInfo Font = Text_Cooldown->GetFont();

	// 이미 MID 상태이면 그대로 사용한다 
	if (UMaterialInstanceDynamic* Existing = Cast<UMaterialInstanceDynamic>(Font.FontMaterial))
	{
		CachedDigitMID = Existing;
		return CachedDigitMID;
	}

	UMaterialInterface* Base = Cast<UMaterialInterface>(Font.FontMaterial);
	if (!Base)
	{
		return nullptr;
	}

	CachedDigitMID = UMaterialInstanceDynamic::Create(Base, this);
	Font.FontMaterial = CachedDigitMID;
	Text_Cooldown->SetFont(Font);
	return CachedDigitMID;
}

UMaterialInstanceDynamic* UShooterSkillSlot::GetIconMID()
{
	if (!CachedIconMID && Image_Icon)
	{
		CachedIconMID = Image_Icon->GetDynamicMaterial();
	}
	return CachedIconMID;
}

void UShooterSkillSlot::SetIconTexture(UTexture2D* InTexture)
{
	if (InTexture == nullptr)
	{
		// null이면 그릴게 없으므로 통쨰로 사라짐
		ResetIconTexture();
		return;
	}

	if (UMaterialInstanceDynamic* MID = GetIconMID())
	{
		MID->SetTextureParameterValue(TEXT("IconTexture"), InTexture);
	}
}