#include "GameMode/State/BombGameState.h"
#include "Actor/DataJack.h"
#include "Net/UnrealNetwork.h"


void ABombGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABombGameState, ActiveDataJack);
}

ADataJack* ABombGameState::GetActiveDataJack() const
{
	return ActiveDataJack;
}

void ABombGameState::SetActiveDataJack(ADataJack* NewActiveDataJack)
{
	if (!HasAuthority() || ActiveDataJack == NewActiveDataJack)
	{
		return;
	}

	ActiveDataJack = NewActiveDataJack;
	OnRep_ActiveDataJack();
	ForceNetUpdate();
}

void ABombGameState::OnRep_ActiveDataJack()
{
	OnActiveDataJackChanged.Broadcast(ActiveDataJack);
}
