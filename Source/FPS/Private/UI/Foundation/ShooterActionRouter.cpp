#include "UI/Foundation/ShooterActionRouter.h"

void UShooterActionRouter::RecalculateActiveInputConfig()
{
	if (const TSharedPtr<FActivatableTreeRoot> ActiveRoot = GetActiveRoot().Pin())
	{
		SetActiveRoot(ActiveRoot);
	}
	RefreshUIInputConfig();
}
