#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "DominationClassInfoPanel.generated.h"

struct FDominationCombatClassDefinition;
class UCommonTextBlock;
class UDominationClassTraitTag;
class UImage;
class UTexture2D;
class UHorizontalBox;

/**
 * 클래스 선택 화면 우측의 정보 패널
 *   - 클래스명 / 주무기명 / 특성 태그 : UDominationClassSettings
 *   - 무기 아이콘                    : 화면의 아이콘 캐시
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class FPS_API UDominationClassInfoPanel : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetDisplayClass(
		const FDominationCombatClassDefinition* Definition,
		UTexture2D* WeaponIcon);

private:
	void RebuildTraitTags(const TArray<FText>& TraitTags);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Emblem;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_ClassName;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> Text_PrimaryWeapon;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> Panel_TraitTags;

	UPROPERTY(EditDefaultsOnly, Category = "Domination|Class Selection")
	TSubclassOf<UDominationClassTraitTag> TraitTagWidgetClass;
};
