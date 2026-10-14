#pragma once

#include "CoreMinimal.h"
#include "Engine/GameEngine.h"
#include "ShooterGameEngine.generated.h"

UCLASS()
class FPS_API UShooterGameEngine : public UGameEngine
{
	GENERATED_BODY()

public:
	virtual bool Experimental_ShouldPreDuplicateMap(FName MapName) const override;
};
