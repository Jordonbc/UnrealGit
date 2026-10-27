// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FUnrealGitTestsModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FUnrealGitTestsModule, UnrealGitTests)

