// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "EditorModeRegistry.h"
#include "climb/ClimbRouteEdMode.h"

#define LOCTEXT_NAMESPACE "FClimbSystemEditorModule"

class FClimbSystemEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FEditorModeRegistry::Get().RegisterMode<FClimbRouteEdMode>(
            FClimbRouteEdMode::EM_ClimbRoute,
            LOCTEXT("ClimbRouteEdModeName", "Climb Route"),
            FSlateIcon(),
            true);
    }

    virtual void ShutdownModule() override
    {
        if (FModuleManager::Get().IsModuleLoaded("UnrealEd"))
        {
            FEditorModeRegistry::Get().UnregisterMode(
                FClimbRouteEdMode::EM_ClimbRoute);
        }
    }
};

IMPLEMENT_MODULE(FClimbSystemEditorModule, ClimbSystemEditor)

#undef LOCTEXT_NAMESPACE
