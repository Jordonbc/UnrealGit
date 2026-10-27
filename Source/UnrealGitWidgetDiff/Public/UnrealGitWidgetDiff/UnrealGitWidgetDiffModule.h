// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class IUnrealGitWidgetDiffModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
