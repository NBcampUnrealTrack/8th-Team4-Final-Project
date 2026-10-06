#include "Modules/ModuleManager.h"

class FFPSAnimationEditorModule final : public IModuleInterface
{
public:
    void StartupModule() override
    {
        // Uncooked -game loads groom assets with their editor Dataflow graphs,
        // but skips automatic Editor-type module loading (GIsEditor is false).
        // Register the existing engine nodes before those graphs deserialize.
        // This UncookedOnly module is excluded from packaged game targets.
        FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("HairStrandsDataflow"));
    }
};

// Also required by the saved animation graph's GroundFeet editor node.
IMPLEMENT_MODULE(FFPSAnimationEditorModule, FPSAnimationEditor)
