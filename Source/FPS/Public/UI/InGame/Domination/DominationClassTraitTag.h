#pragma once

#include "CoreMinimal.h"
#include "CommonTextBlock.h"
#include "CommonUserWidget.h"
#include "DominationClassTraitTag.generated.h"

/**
 * 우측 정보 패널의 특성 태그 한 줄
 */
UCLASS(Abstract)
class FPS_API UDominationClassTraitTag : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetTraitText(const FText& InText)
	{
		if (IsValid(Text_Trait))
		{
			Text_Trait->SetText(InText);
		}
	}

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Trait;
};
