#include "UI/Foundation/ShooterActionRouter.h"

void UShooterActionRouter::RecalculateActiveInputConfig()
{
#if WITH_EDITOR
	if (const TSharedPtr<FActivatableTreeRoot> ActiveRoot = GetActiveRoot().Pin())
	{
		SetActiveRoot(ActiveRoot);
	}
#endif
}
