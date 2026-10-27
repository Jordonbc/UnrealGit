// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// The plugin's only log category. Default verbosity is Warning; verbose output
// is only produced when explicitly enabled at runtime.
DECLARE_LOG_CATEGORY_EXTERN(LogUnrealGit, Warning, All);

class FAutoConsoleCommand;
class FUnrealGitSourceControlProvider;

class FUnrealGitModule final : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TUniquePtr<FUnrealGitSourceControlProvider> Provider;
	TUniquePtr<FAutoConsoleCommand> StatusCommand;
};
