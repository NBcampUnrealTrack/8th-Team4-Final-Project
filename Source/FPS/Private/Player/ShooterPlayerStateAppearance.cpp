#include "Player/ShooterPlayerState.h"
#include "Customization/GameplayAppearanceComponent.h"
#include "Customization/FirstPersonAppearanceComponent.h"
#include "GameFramework/Pawn.h"

bool AShooterPlayerState::StoreValidatedAppearance(FName ClassId, const FCustomizationAppearance& Appearance)
{
    if (!HasAuthority() || !Appearance.ValidateForClass(ClassId.ToString())) return false;
    FReplicatedAppearance* Existing = SavedAppearances.FindByPredicate([ClassId](const auto& Row){ return Row.ClassId == ClassId; });
    if (Existing && FCustomizationAppearance::StaticStruct()->CompareScriptStruct(&Existing->Appearance, &Appearance, 0)) return true;
    if (!Existing)
    {
        if (SavedAppearances.Num() >= 3) return false;
        Existing = &SavedAppearances.AddDefaulted_GetRef();
        Existing->ClassId = ClassId;
    }
    Existing->Appearance = Appearance;
    ++Existing->Revision;
    if (Existing->Revision == 0) ++Existing->Revision;
    OnRep_Appearances();
    ForceNetUpdate();
    return true;
}

const FReplicatedAppearance* AShooterPlayerState::FindAppearance(FName ClassId) const
{
    return SavedAppearances.FindByPredicate([ClassId](const auto& Row){ return Row.ClassId == ClassId; });
}

void AShooterPlayerState::OnRep_Appearances()
{
    if (APawn* Pawn = GetPawn())
    {
        if (auto* Component = Pawn->FindComponentByClass<UGameplayAppearanceComponent>()) Component->RefreshFromPlayerState();
        if (auto* Component = Pawn->FindComponentByClass<UFirstPersonAppearanceComponent>()) Component->RefreshFromPlayerState();
    }
}

void AShooterPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
    Super::CopyProperties(NewPlayerState);
    if (auto* NewState = Cast<AShooterPlayerState>(NewPlayerState)) NewState->SavedAppearances = SavedAppearances;
}

void AShooterPlayerState::OverrideWith(APlayerState* PreviousPlayerState)
{
    Super::OverrideWith(PreviousPlayerState);
    if (auto* Previous = Cast<AShooterPlayerState>(PreviousPlayerState)) SavedAppearances = Previous->SavedAppearances;
    OnRep_Appearances();
}
