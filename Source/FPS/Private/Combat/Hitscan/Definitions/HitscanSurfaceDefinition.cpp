#include "Combat/Hitscan/Definitions/HitscanSurfaceDefinition.h"

FHitscanSurfaceRule UHitscanSurfaceDefinition::ResolveSurfaceRule(
	const TEnumAsByte<EPhysicalSurface> SurfaceType) const
{
	for (const FHitscanPhysicalSurfaceRule& Entry : SurfaceRules)
	{
		if (Entry.SurfaceType == SurfaceType)
		{
			return Entry.SurfaceRule;
		}
	}

	return DefaultSurfaceRule;
}
