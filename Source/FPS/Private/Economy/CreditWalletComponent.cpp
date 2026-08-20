#include "Economy/CreditWalletComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogCreditWallet, Log, All);

UCreditWalletComponent::UCreditWalletComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCreditWalletComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 크레딧은 소유 플레이어에게만 복제하고 변경 시 항상 OnRep를 호출한다.
	DOREPLIFETIME_CONDITION_NOTIFY(
		UCreditWalletComponent,
		Credits,
		COND_OwnerOnly,
		REPNOTIFY_Always);
}

bool UCreditWalletComponent::CanAfford(const int32 Cost) const
{
	return Cost >= 0 && Credits >= Cost;
}

bool UCreditWalletComponent::CanAddCredits(const int32 Amount) const
{
	return Amount >= 0 && Credits <= MAX_int32 - Amount;
}

bool UCreditWalletComponent::ResetCredits(const int32 NewCredits, const ECreditChangeReason Reason)
{
	if (!HasServerAuthority() || NewCredits < 0)
	{
		return false;
	}

	SetCreditsInternal(NewCredits, Reason);
	return true;
}

bool UCreditWalletComponent::AddCredits(const int32 Amount, const ECreditChangeReason Reason)
{
	if (!HasServerAuthority() || !CanAddCredits(Amount))
	{
		return false;
	}

	SetCreditsInternal(Credits + Amount, Reason);
	return true;
}

bool UCreditWalletComponent::TrySpendCredits(const int32 Cost, const ECreditChangeReason Reason)
{
	if (!HasServerAuthority() || !CanAfford(Cost))
	{
		return false;
	}

	SetCreditsInternal(Credits - Cost, Reason);
	return true;
}

void UCreditWalletComponent::OnRep_Credits(const int32 PreviousCredits)
{
	// 클라이언트에서는 복제된 값 변경을 동일한 델리게이트로 전달한다.
	OnCreditsChanged.Broadcast(PreviousCredits, Credits);
}

bool UCreditWalletComponent::HasServerAuthority() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && Owner->HasAuthority();
}

void UCreditWalletComponent::SetCreditsInternal(const int32 NewCredits, const ECreditChangeReason Reason)
{
	if (Credits == NewCredits)
	{
		return;
	}

	const int32 PreviousCredits = Credits;
	Credits = NewCredits;

	UE_LOG(
		LogCreditWallet,
		Verbose,
		TEXT("%s credits changed: %d -> %d (%s)"),
		*GetNameSafe(GetOwner()),
		PreviousCredits,
		Credits,
		*UEnum::GetValueAsString(Reason));

	// 서버에서도 즉시 동일한 변경 이벤트를 발생시켜 UI/시스템이 한 경로로 처리하게 한다.
	OnCreditsChanged.Broadcast(PreviousCredits, Credits);

	GetOwner()->ForceNetUpdate();
}