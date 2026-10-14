#include "Interfaces/RogueCombatFeedbackPolicy.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

namespace
{
    const IRogueCombatFeedbackPolicy* ResolveDirect(const AActor* Actor)
    {
        if (!IsValid(Actor))
        {
            return nullptr;
        }

        if (const IRogueCombatFeedbackPolicy* Policy = Cast<IRogueCombatFeedbackPolicy>(const_cast<AActor*>(Actor)))
        {
            return Policy;
        }

        TInlineComponentArray<UActorComponent*> Components;
        Actor->GetComponents(Components);
        for (UActorComponent* Component : Components)
        {
            if (const IRogueCombatFeedbackPolicy* Policy = Cast<IRogueCombatFeedbackPolicy>(Component))
            {
                return Policy;
            }
        }
        return nullptr;
    }
}

const IRogueCombatFeedbackPolicy* RogueCombatFeedbackPolicy::Resolve(const AActor* Actor)
{
    if (const IRogueCombatFeedbackPolicy* Policy = ResolveDirect(Actor))
    {
        return Policy;
    }

    const APawn* Pawn = Cast<APawn>(Actor);
    return Pawn ? ResolveDirect(Pawn->GetController()) : nullptr;
}
