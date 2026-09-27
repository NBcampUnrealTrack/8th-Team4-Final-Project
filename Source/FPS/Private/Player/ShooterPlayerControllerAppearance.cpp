#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "Customization/AppearanceLocalPlayerSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

void AShooterPlayerController::AcknowledgePossession(APawn* NewPawn)
{
    Super::AcknowledgePossession(NewPawn);
    if (!IsLocalController()) return;
    // Re-read completed downloads only at a spawn boundary, never mid-combat.
    bAppearanceSubmitted = false; AppearanceSubmitAttempts = 0;
    GetWorldTimerManager().SetTimer(AppearanceSubmitTimer, this, &ThisClass::SubmitSavedAppearances, 1.05f, false);
}

void AShooterPlayerController::SubmitSavedAppearances()
{
    if (!IsLocalController() || bAppearanceSubmitted || !GetWorld()) return;
    auto* Local = GetLocalPlayer();
    if (!Local || !GetPlayerState<AShooterPlayerState>())
    {
        if (++AppearanceSubmitAttempts <= 30)
            GetWorldTimerManager().SetTimer(AppearanceSubmitTimer, this, &ThisClass::SubmitSavedAppearances, .5f, false);
        return;
    }
    auto* Storage = Local->GetSubsystem<UAppearanceLocalPlayerSubsystem>();
    if (!Storage) return;
    Storage->RefreshIdentity();
    if (Storage->GetAccount().IsEmpty())
    {
        if (++AppearanceSubmitAttempts <= 30)
            GetWorldTimerManager().SetTimer(AppearanceSubmitTimer, this, &ThisClass::SubmitSavedAppearances, .5f, false);
        return;
    }
    for (const FName ClassId : {FName(TEXT("Ranger")), FName(TEXT("Trooper")), FName(TEXT("Vanguard"))})
    {
        FCustomizationAppearance Appearance;
        if (Storage->LoadSavedAppearance(ClassId, Appearance) == ECustomizationLoadResult::Loaded)
            ServerSubmitAppearance(ClassId, Appearance);
    }
    bAppearanceSubmitted = true;
    GetWorldTimerManager().ClearTimer(AppearanceSubmitTimer);
}

void AShooterPlayerController::ServerSubmitAppearance_Implementation(FName ClassId, const FCustomizationAppearance& Appearance)
{
    const double Now = GetWorld()->GetRealTimeSeconds();
    if (AppearanceRateWindow < 0.0 || Now - AppearanceRateWindow >= 1.0)
    {
        AppearanceRateWindow = Now;
        AppearanceRateCount = 0;
    }
    if (++AppearanceRateCount > 3 || !Appearance.ValidateForClass(ClassId.ToString())) return;
    if (auto* State = GetPlayerState<AShooterPlayerState>()) State->StoreValidatedAppearance(ClassId, Appearance);
}
