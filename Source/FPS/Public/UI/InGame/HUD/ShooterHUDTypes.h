// ShooterHUDTypes.h
#pragma once

#include "CoreMinimal.h"
#include "ShooterHUDTypes.generated.h"

UENUM(BlueprintType)
enum class EShooterHUDVisualTypes : uint8
{
	Normal,
	Warning,
	Critical
};

USTRUCT(BlueprintType)
struct FShooterAmmoVisualStyle
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor WeaponIconColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor WeaponNameColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor CurrentAmmoColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor MaxAmmoColor = FLinearColor::White;

	// 현재 없어도 추후 탄약 아이콘용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor AmmoIconColor = FLinearColor::White;

	// HUD 박스/패널 배경
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor PanelColor = FLinearColor::White;

	// 외곽선, 프레임, 장식선
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor FrameColor = FLinearColor::White;
	
	// 강조선, 작은 장식, 포인트 요소
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FLinearColor AccentColor = FLinearColor::White;
};

USTRUCT(BlueprintType)
struct FShooterAmmoVisualStyleSet
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FShooterAmmoVisualStyle Normal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FShooterAmmoVisualStyle Warning;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FShooterAmmoVisualStyle Critical;

	const FShooterAmmoVisualStyle& GetStyle(EShooterHUDVisualTypes Type) const
	{
		switch (Type)
		{
		case EShooterHUDVisualTypes::Warning:
			return Warning;

		case EShooterHUDVisualTypes::Critical:
			return Critical;

		case EShooterHUDVisualTypes::Normal:
		default:
			return Normal;
		}
	}
};