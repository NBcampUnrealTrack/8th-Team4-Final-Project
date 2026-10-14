#include "Customization/CustomizationStudioGameMode.h"
#include "Customization/CustomizationStudioController.h"
ACustomizationStudioGameMode::ACustomizationStudioGameMode(){PlayerControllerClass=ACustomizationStudioController::StaticClass();DefaultPawnClass=nullptr;HUDClass=nullptr;}
