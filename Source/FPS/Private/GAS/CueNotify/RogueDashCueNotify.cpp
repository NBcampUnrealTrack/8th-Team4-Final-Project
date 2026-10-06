#include "GAS/CueNotify/RogueDashCueNotify.h"

#include "Animation/AnimMontage.h"
#include "Character/ShooterCharacter.h"
#include "Combat/CombatComponent.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Player/DominationPlayerState.h"
#include "Tags/GameplayTags.h"

UAnimMontage* URogueDashCueNotify::ResolveMontage3P(AActor* Target) const
{
	const auto* Character = Cast<AShooterCharacter>(Target);
	const auto* Combat = Character ? Character->GetCombatComponent() : nullptr;
	if (!Combat || !Combat->GetCurrentWeapon()) return Super::ResolveMontage3P(Target);

	const auto* State = Character->GetPlayerState<ADominationPlayerState>();
	EDominationCombatClass Class = State ? State->GetCombatClass() : EDominationCombatClass::Unselected;
	// Cosmetic mesh replacement and late PlayerState replication must not select a different grip.
	if (Class == EDominationCombatClass::Unselected)
		for (const auto& Definition : GetDefault<UDominationClassSettings>()->CombatClasses)
			if (Definition.PawnClass.Get() && Character->IsA(Definition.PawnClass.Get()))
			{
				Class = Definition.CombatClass;
				break;
			}

	const FDashClassMontages* Fitted = ClassMontages.Find(Class);
	if (!Fitted) return Super::ResolveMontage3P(Target);
	const FGameplayTag Type = Combat->GetCurrentWeaponTag();
	UAnimMontage* Montage = nullptr;
	if (Type.MatchesTag(RogueTags::Weapon_Type_Knife)) Montage = Fitted->Knife;
	else if (Type.MatchesTag(RogueTags::Weapon_Type_Pistol)) Montage = Fitted->Pistol;
	else if (const auto* Definition = GetDefault<UDominationClassSettings>()->FindDefinition(Class);
		Definition && Type.MatchesTag(Definition->PrimaryWeaponTag)) Montage = Fitted->Primary;
	return Montage ? Montage : Super::ResolveMontage3P(Target);
}
