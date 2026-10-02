#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GameMode/Types/DominationTypes.h"

#include "RogueVoiceCueNotify.generated.h"

class USoundBase;

USTRUCT()
struct FRogueClassVoice
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Voice")
	TObjectPtr<USoundBase> SelfVoice;

	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Voice")
	TObjectPtr<USoundBase> AllyVoice;

	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Voice")
	TObjectPtr<USoundBase> EnemyVoice;
};

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

	// 공용 스킬용. 시전자 직업이 있으면 위 세 칸 대신 사용
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Voice")
	TMap<EDominationCombatClass, FRogueClassVoice> ClassVoices;
};
