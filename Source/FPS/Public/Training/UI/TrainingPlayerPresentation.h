#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Training/TrainingTypes.h"
#include "Training/UI/TrainingStartFeedbackWidget.h"
#include "Training/UI/TrainingSessionHUDWidget.h"
#include "Training/UI/TrainingDamageFeedbackWidget.h"
#include "TrainingPlayerPresentation.generated.h"

class UTrainingPlayerComponent;
class USoundAttenuation;
class USoundBase;

UCLASS()
class FPS_API UTrainingPlayerPresentation : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UTrainingPlayerComponent& InOwner);
    void HandlePhase(int32 SessionId, ETrainingFeedbackPhase Phase, double PhaseStartSeconds,
        ETrainingMode Mode, float DurationSeconds);
    void HandleHit(int32 SessionId, int32 Kills, float Damage, bool bKilled, const FVector& WorldPosition);
    void PresentStandaloneDamage(float Damage, bool bKilled, const FVector& WorldPosition);
    void PlayBotSound(int32 SessionId, USoundBase* Sound, USoundAttenuation* Attenuation,
        const FVector& WorldPosition, float Volume);
    void Clear();
    int32 GetSessionId() const { return FeedbackSessionId; }

private:
    UTrainingDamageFeedbackWidget* EnsureDamageFeedbackWidget();
    void PresentDamage(float Damage, bool bKilled, const FVector& WorldPosition);

    TWeakObjectPtr<UTrainingPlayerComponent> Owner;

    UPROPERTY(Transient)
    TObjectPtr<UTrainingStartFeedbackWidget> StartFeedbackWidget;

    UPROPERTY(Transient)
    TObjectPtr<UTrainingSessionHUDWidget> SessionHUDWidget;

    UPROPERTY(Transient)
    TObjectPtr<UTrainingDamageFeedbackWidget> DamageFeedbackWidget;

    UPROPERTY(Transient)
    TObjectPtr<USoundAttenuation> DefaultBotAttenuation;

    int32 FeedbackSessionId = 0;
};
