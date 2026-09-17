#include "Combat/Hitscan/Definitions/HitscanSurfaceDefinition.h"

#include "Particles/ParticleSystem.h"

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

UParticleSystem* UHitscanSurfaceDefinition::ResolveImpactParticle(const TEnumAsByte<EPhysicalSurface> SurfaceType, const bool bHeadshot) const
{
	for (const FHitscanPhysicalSurfaceRule& Entry : SurfaceRules)
	{
		if (Entry.SurfaceType == SurfaceType)
		{
			if (bHeadshot && Entry.HeadshotImpactParticle)
			{
				return Entry.HeadshotImpactParticle;
			}

			if (!Entry.ImpactParticleVariants.IsEmpty())
			{
				const int32 RandomIndex = FMath::RandHelper(Entry.ImpactParticleVariants.Num());
				if (UParticleSystem* Variant = Entry.ImpactParticleVariants[RandomIndex])
				{
					return Variant;
				}
			}

			return Entry.ImpactParticle ? Entry.ImpactParticle : DefaultImpactParticle;
		}
	}

	return DefaultImpactParticle;
}
