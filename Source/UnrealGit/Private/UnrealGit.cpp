// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit.h"

#include "ISourceControlModule.h"
#include "UnrealGit/SourceControl/UnrealGitSourceControlProvider.h"

#define LOCTEXT_NAMESPACE "FUnrealGitModule"

void FUnrealGitModule::StartupModule()
{
	ISourceControlModule& SourceControlModule = FModuleManager::LoadModuleChecked<ISourceControlModule>("SourceControl");
	SourceControlModule.RegisterProvider(MakeShared<FUnrealGitSourceControlProvider, ESPMode::ThreadSafe>());
}

void FUnrealGitModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("SourceControl"))
	{
		ISourceControlModule::Get().UnregisterProvider("UnrealGit");
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealGitModule, UnrealGit)
