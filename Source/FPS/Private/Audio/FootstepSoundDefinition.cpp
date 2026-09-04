#include "Audio/FootstepSoundDefinition.h"

const FFootstepSurfaceSound* UFootstepSoundDefinition::ResolveFootstepSound(
	const TEnumAsByte<EPhysicalSurface> SurfaceType) const
{
	for (const FFootstepSurfaceSound& Entry : SurfaceSounds)
	{
		if (Entry.SurfaceType == SurfaceType && Entry.Sound)
		{
			return &Entry;
		}
	}

	return DefaultSound.Sound ? &DefaultSound : nullptr;
}
