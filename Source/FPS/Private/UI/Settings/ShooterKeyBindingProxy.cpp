#include "UI/Settings/ShooterKeyBindingProxy.h"

#include "UI/Settings/ShooterOptionsScreen.h"

void UShooterKeyBindingProxy::Initialize(UShooterOptionsScreen* InOwner, const FName InBindingId)
{
	Owner = InOwner;
	BindingId = InBindingId;
}

void UShooterKeyBindingProxy::HandleKeySelected(const FInputChord SelectedKey)
{
	if (UShooterOptionsScreen* Screen = Owner.Get())
	{
		Screen->HandleKeyBindingSelected(BindingId, SelectedKey.Key);
	}
}
