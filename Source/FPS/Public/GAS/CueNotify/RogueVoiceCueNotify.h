#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"

#include "RogueVoiceCueNotify.generated.h"

class USoundBase;

UCLASS()
class FPS_API URogueVoiceCueNotify : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Voice")
	TObjectPtr<USoundBase> SelfVoice;

	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Voice")
	TObjectPtr<USoundBase> AllyVoice;

	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Voice")
	TObjectPtr<USoundBase> EnemyVoice;
};
