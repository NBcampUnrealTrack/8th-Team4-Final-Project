#include "GAS/CueNotify/RogueVoiceCueNotify.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/DominationPlayerState.h"
#include "Sound/SoundBase.h"

bool URogueVoiceCueNotify::OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const
{
	AActor* Caster = Parameters.Instigator.IsValid() ? Parameters.Instigator.Get() : Target;
	UWorld* World = IsValid(Caster) ? Caster->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	const APawn* CasterPawn = Cast<APawn>(Caster);

	const ADominationPlayerState* CasterClassState = CasterPawn ? CasterPawn->GetPlayerState<ADominationPlayerState>() : nullptr;
	const FRogueClassVoice* ClassVoice = CasterClassState ? ClassVoices.Find(CasterClassState->GetCombatClass()) : nullptr;

	if (CasterPawn && CasterPawn->IsLocallyControlled())
	{
		if (USoundBase* Voice = ClassVoice ? ClassVoice->SelfVoice.Get() : SelfVoice.Get())
		{
			UGameplayStatics::SpawnSound2D(World, Voice);
		}
		return true;
	}

	const APlayerController* Viewer = World->GetFirstPlayerController();
	const ABaseTeamPlayerState* ViewerState = Viewer ? Viewer->GetPlayerState<ABaseTeamPlayerState>() : nullptr;
	const ABaseTeamPlayerState* CasterState = CasterPawn ? CasterPawn->GetPlayerState<ABaseTeamPlayerState>() : nullptr;

	const bool bAlly = ViewerState && CasterState
		&& ViewerState->GetTeam() != EGameTeam::None
		&& ViewerState->GetTeam() == CasterState->GetTeam();

	if (USoundBase* Voice = ClassVoice
		? (bAlly ? ClassVoice->AllyVoice : ClassVoice->EnemyVoice).Get()
		: (bAlly ? AllyVoice : EnemyVoice).Get())
	{
		UGameplayStatics::SpawnSoundAttached(Voice, Caster->GetRootComponent());
	}
	return true;
}
