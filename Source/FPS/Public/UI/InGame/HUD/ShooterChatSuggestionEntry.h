// ShooterChatSuggestionEntry.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ShooterChatSuggestionEntry.generated.h"

class UCommonTextBlock;

// 귓속말(/w) 자동완성 드롭다운 한 줄. UShooterChatInputBox가 매칭되는 플레이어 이름 개수만큼
// 동적으로 생성해서 Box_Suggestions에 채운다. 마우스 클릭이 아니라 방향키+Enter로만 고른다
// (오버워치 친구 검색과 동일) - 그래서 버튼이 아니라 텍스트 하이라이트만 있다.
UCLASS()
class FPS_API UShooterChatSuggestionEntry : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetPlayerName(const FString& InPlayerName);
	void SetHighlighted(bool bInHighlighted);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Name;

	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	FLinearColor NormalColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	FLinearColor HighlightedColor = FLinearColor(0.75f, 0.3f, 0.9f);
};
