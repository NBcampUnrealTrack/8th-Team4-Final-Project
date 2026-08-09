
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

UCLASS()
class FPS_API AWeaponBase : public AActor
{
	GENERATED_BODY()
	
public:	
	AWeaponBase();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	FGameplayTag WeaponType;

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

};
