// ShooterChatLog.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Chat/ChatTypes.h"
#include "ShooterChatLog.generated.h"

class AShooterPlayerController;
class UShooterChatEntry;
class UVerticalBox;

// 화면 좌측 상단에 상시 표시되는 채팅 로그. UShooterKillFeed와 동일한 구조(자가 구독 +
// VerticalBox 엔트리 + 타이머 체인 만료)를 따른다.
UCLASS()
class FPS_API UShooterChatLog : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

protected:
	virtual void NativeDestruct() override;

	void RemoveOldestEntry();

	UFUNCTION()
	void HandleChatMessageReceived(const FChatMessage& Message);

	UPROPERTY(EditDefaultsOnly, Category = "Chat", meta = (ClampMin = "1"))
	int32 MaxVisibleEntries = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Chat", meta = (ClampMin = "0.1"))
	float EntryLifetimeSeconds = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	TSubclassOf<UShooterChatEntry> EntryWidgetClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> Box_Entries;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UShooterChatEntry>> ActiveEntries;

	TWeakObjectPtr<AShooterPlayerController> BoundPlayerController;

	FTimerHandle ExpiryTimerHandle;
};
