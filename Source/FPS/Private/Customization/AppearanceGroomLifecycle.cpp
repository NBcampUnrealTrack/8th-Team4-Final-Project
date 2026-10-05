#include "Customization/AppearanceGroomLifecycle.h"
#include "GroomComponent.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"

void AppearanceGroomLifecycle::Clear(UGroomComponent& Component)
{
    Component.SetVisibility(false, false);
    // Nulling the asset alone does not tear down Niagara or clear the old binding.
    if (Component.IsRegistered()) Component.UnregisterComponent();
    Component.SetComponentTickEnabled(false);
    Component.SetGroomAsset(nullptr, nullptr);
    Component.SetGroomCache(nullptr);
    Component.EmptyOverrideMaterials();
}

void AppearanceGroomLifecycle::Activate(UGroomComponent& Component, USceneComponent& Parent,
    UGroomAsset& Groom, UGroomBindingAsset& Binding, bool bUseCards)
{
    Clear(Component);
    Component.AttachToComponent(&Parent, FAttachmentTransformRules::KeepRelativeTransform);
    Component.SetUseCards(bUseCards);
    Component.SetGroomAsset(&Groom, &Binding, false);
    Component.RegisterComponent(); // Registration recreates simulation for the selected groom.
    Component.SetComponentTickEnabled(true);
    Component.SetVisibility(true, false);
}
