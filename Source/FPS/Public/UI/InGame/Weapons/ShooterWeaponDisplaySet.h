// ShooterWeaponDisplaySet.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "ShooterWeaponDisplaySet.generated.h"

USTRUCT(BlueprintType)
struct FShooterWeaponDisplay
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FText DisplayName;
	// UTexture2D가 아니라 브러시다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FSlateBrush Icon;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	FSlateBrush AmmonIcon;
};

UCLASS(BlueprintType)
class FPS_API UShooterWeaponDisplaySet : public UDataAsset
{
	GENERATED_BODY()
	
public:
	// 못찾으면 nullptr, 그때는 이름/아이콘을 그리지 않고 탄약만 표시
	const FShooterWeaponDisplay* FindDisplay(FGameplayTag WeaponType) const
	{
		return Displays.Find(WeaponType);
	}
	
	// Killlog가 WeaponTag로 아이콘을 찾을 때 사용
	UFUNCTION(BlueprintPure, Category = "UI")
	bool FindWeaponDisplay(FGameplayTag WeaponType, FShooterWeaponDisplay& OutDisplay) const
	{
		if (const FShooterWeaponDisplay* Found = FindDisplay(WeaponType))
		{
			OutDisplay = *Found;
			return true;
		}
		return false;
	}
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TMap<FGameplayTag, FShooterWeaponDisplay> Displays;
};