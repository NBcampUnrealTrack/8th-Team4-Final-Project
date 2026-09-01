#include "Abilities/GA_DataJackDrop.h"

#include "GameMode/Manager/BombObjectiveManager.h"
#include "GameFramework/PlayerController.h"
#include "Tags/GameplayTags.h"

UGA_DataJackDrop::UGA_DataJackDrop()
{
	ActivationOwnedTags.AddTag(RogueTags::State_Combat_DataJack_Dropping);
}

EDataJackOperation UGA_DataJackDrop::GetDataJackOperation() const
{
	return EDataJackOperation::Drop;
}

bool UGA_DataJackDrop::BeginDataJackOperation(
	UBombObjectiveManager& ObjectiveManager,
	APlayerController& PlayerController)
{
	return ObjectiveManager.BeginDrop(&PlayerController);
}

void UGA_DataJackDrop::CancelDataJackOperation(
	UBombObjectiveManager& ObjectiveManager,
	APlayerController& PlayerController)
{
	ObjectiveManager.CancelDrop(&PlayerController);
}
