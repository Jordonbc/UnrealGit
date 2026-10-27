// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

/**
 * @class IUnrealGitWidgetDiffModule
 * Module interface for the UnrealGitWidgetDiff editor module.
 */
class IUnrealGitWidgetDiffModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
