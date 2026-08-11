// ShooterAugmentTierStyleSet.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Augment/AugmentTypes.h"
#include "ShooterAugmentTierStyleSet.generated.h"

class UCommonButtonStyle;

// 각 등급마다 표시에 필요한 데이터
USTRUCT(BlueprintType)
struct FShooterAugmentTierStyle{
	GENERATED_BODY()

	// 이 등급의 카드 버튼 스타일, Frame / Hover / pressed
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UCommonButtonStyle> ButtonStyle;
	
	// 이름, 아이콘, 등급 Label 색깔
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FLinearColor AccentColor = FLinearColor::White;
	
	// 화면에 띄울 등급 이름 Legendary -> 전설 같이 이름을 정함
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FText TierDisplayName;
	
	// 등장할 때 강조 애니메이션 여부, 현재는 Legendary만 true
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	bool bPlayHighlightIntro = false;
	
	// 등장할 때 강조 연출 여부, 현재는 Legendary만 true
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	bool bShowAlertBadge = false;
};

// 증강 등급별 표시 규격, 등급 색이나 프레임을 바꾸려면 여기만 수정
UCLASS(BlueprintType)
class FPS_API UShooterAugmentTierStyleSet : public UDataAsset{
	GENERATED_BODY()
	
public:
	const FShooterAugmentTierStyle* FindStyle(EAugmentTier Tier)const{
		return Styles.Find(Tier);
	}
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TMap<EAugmentTier, FShooterAugmentTierStyle> Styles;
};
