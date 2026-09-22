// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FireLane - the friendly-fire and line-of-fire gate.
 *
 * The module itself only registers two console commands; everything the plugin does lives in the
 * component, the subsystem and the pure rules.
 */
class FFireLaneModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
