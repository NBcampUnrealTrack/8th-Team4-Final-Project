// ShooterTeamStatus.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameMode/TeamTypes.h"
#include "ShooterTeamStatus.generated.h"

class ABaseRoundGameState;
class UShooterPlayerSlot;
class UHorizontalBox;

/**
 * 아군/적군 팀의 생존 아이콘 컨테이너
 * 같은 클래스를 캔버스에 두개 놓고 bShowLocalTeam으로 아군/적군을 가름
 */
UCLASS()
class FPS_API UShooterTeamStatus : public UCommonUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleRoundStateChanged();

	void RebuildRoster();
	
	// 슬롯이 부족하면 만들고 이미 있으면 그걸 준다
	UShooterPlayerSlot* GetOrCreateSlot(int32 Index);
	
	ABaseRoundGameState* GetRoundGameState() const;

	// 로컬 플레이어의 팀. 미배정이면 EGameTeam::None반환
	EGameTeam GetLocalTeam() const;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shooter|UI")
	bool bShowLocalTeam = true;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|UI")
	FLinearColor AllyColor = FLinearColor(0.2f, 0.6f, 1.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|UI")
	FLinearColor EnemyColor = FLinearColor(1.0f, 0.3f, 0.3f);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|UI")
	TSubclassOf<UShooterPlayerSlot> SlotWidgetClass;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> Box_Slots;
	
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Shooter|UI|Debug")
	void DebugFillSlots(int32 Count = 4);

	
private:
	// 만들어 둔 슬롯을 라운드마다 재사용
	UPROPERTY(Transient)
	TArray<TObjectPtr<UShooterPlayerSlot>> Slots;
	
	TWeakObjectPtr<ABaseRoundGameState> BoundGameState;
};
