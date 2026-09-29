#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "SystemAnnouncer.generated.h"

class UAudioComponent;
class USoundBase;
class USoundClass;

USTRUCT(BlueprintType)
struct FPS_API FSystemAnnouncerCue
{
	GENERATED_BODY()

	// 반복 재생하지 않는 음성 에셋을 지정한다. 미지정 항목은 건너뛴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcer")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcer", meta = (ClampMin = "0"))
	int32 Priority = 0;

	// 높은 우선순위의 안내가 현재 음성을 끊고, 낮은 우선순위의 대기 음성도 버린다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcer")
	bool bInterruptLowerPriority = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcer", meta = (ClampMin = "0.0", Units = "s"))
	float CooldownSeconds = 1.0f;

	// 상황이 지난 뒤 밀린 안내가 재생되지 않도록 대기 시간을 제한한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcer", meta = (ClampMin = "0.1", Units = "s"))
	float MaxQueueDelaySeconds = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Announcer", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;
};

// 게임 규칙을 모르는 공용 방송기다. 서버는 사건 ID만 전달하고 각 월드는 한 음성씩 재생한다.
UCLASS(Blueprintable)
class FPS_API ASystemAnnouncer : public AInfo
{
	GENERATED_BODY()

public:
	ASystemAnnouncer();

	// 서버에서만 호출한다. 클라이언트가 임의로 전체 방송을 요청하는 RPC는 제공하지 않는다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Announcer")
	bool Announce(FName AnnouncementId);

	// 이 머신에서만 재생한다(서버 방송 없음). 팀에 따라 내용이 달라지는 안내처럼 각 클라이언트가
	// 자기 기준으로 ID를 골라야 할 때 사용한다. 대기열·우선순위·음성 볼륨 분류는 서버 방송과 동일하다.
	UFUNCTION(BlueprintCallable, Category = "Announcer")
	void AnnounceLocal(FName AnnouncementId);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Announcer")
	void StopAnnouncements();

	UFUNCTION(BlueprintPure, Category = "Announcer")
	bool HasAnnouncement(FName AnnouncementId) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 복제 상태로 복원하는 안내도 기존 음성 대기열을 사용한다.
	void QueueAnnouncement(FName AnnouncementId, float QueueLifetimeSeconds = -1.0f);
	void CancelAnnouncement(FName AnnouncementId);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Announcer")
	TMap<FName, FSystemAnnouncerCue> Announcements;

	// 비워두면 기존 Shooter Audio Settings의 Voice Sound Class를 사용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Announcer")
	TObjectPtr<USoundClass> VoiceSoundClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Announcer", meta = (ClampMin = "1", ClampMax = "32"))
	int32 MaxQueuedAnnouncements = 8;

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastAnnounce(FName AnnouncementId);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopAnnouncements();

	UFUNCTION()
	void HandleVoiceFinished();

	void PlayNextAnnouncement();
	bool TryPlayAnnouncement(FName AnnouncementId, const FSystemAnnouncerCue& Cue);
	void StopCurrentVoice();
	void ClearLocalAnnouncements();

	struct FQueuedAnnouncement
	{
		FName Id;
		double ExpiresAt = 0.0;
	};

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> VoiceAudioComponent;

	TArray<FQueuedAnnouncement> PendingAnnouncements;
	TMap<FName, double> LastSentTimes;
	FName CurrentAnnouncementId;
	int32 CurrentPriority = 0;
	bool bShuttingDown = false;
};
