// TeammateIndicatorLayer.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GameMode/Types/TeamTypes.h"
#include "TeammateIndicatorLayer.generated.h"

class ABombGameState;
class ABaseRoundGameState;
class UCanvasPanel;
class UTeammateIndicatorEntry;
class ABaseTeamPlayerState;
class ADataJack;

/**
 * 아군 오버헤드 인디케이터 컨테이너
 * 로컬 팀 인원만큼 Entry 만들어서 관리
 */
UCLASS()
class FPS_API UTeammateIndicatorLayer : public UCommonUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	void BindToGameState();
	void UnbindFromGameState();
	
	UFUNCTION()
	void HandleRoundStateChanged();
	// 로컬 플레이어의 팀이 정해지는 순간 명단을 다시 만든다
	// HUD가 팀 복제보다 먼저 생성될 수 있어서 필요하다
	UFUNCTION()
	void HandleLocalTeamChanged(EGameTeam previousTeam, EGameTeam NewTeam);
	
	UFUNCTION()
	void HandleActiveDataJackChanged(ADataJack* NewActiveDataJack);
	UFUNCTION()
	void HandleDataJackCarrierChanged();
	
	void RefreshJackCarrierDisplay();
	
	// 로컬 팀(본인 제외)만 골라 Entry를 채운다.
	void RebuildRoster();
	
	// Entry가 부족하면 만든다.
	UTeammateIndicatorEntry* GetOrCreateEntry(int32 Index);
	
	ABaseRoundGameState* GetRoundGameState() const;
	
	// 미배정이면 EGameTeam::None 반환
	EGameTeam GetLocalTeam() const;
	
	UPROPERTY(EditDefaultsOnly, Category = "Shooter|UI")
	TSubclassOf<UTeammateIndicatorEntry> EntryWidgetClass;
	
	// Entry는 이 Canvas의 자식이여야 함.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> Box_Indicators;
	
private:
	// 만들어 둔 Entry를 라운드마다 재사용
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTeammateIndicatorEntry>> Entries;
	
	TWeakObjectPtr<ABaseRoundGameState> BoundGameState;
	TWeakObjectPtr<ABaseTeamPlayerState> BoundLocalPlayerState;

	TWeakObjectPtr<ABombGameState> BoundBombGameState;
	TWeakObjectPtr<ADataJack> BoundDataJack;
};
