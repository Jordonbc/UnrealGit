// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit.h"

#include "Features/IModularFeatures.h"
#include "UnrealGit/SourceControl/UnrealGitSourceControlProvider.h"

#define LOCTEXT_NAMESPACE "FUnrealGitModule"

void FUnrealGitModule::StartupModule()
{
	Provider = MakeUnique<FUnrealGitSourceControlProvider>();
	IModularFeatures::Get().RegisterModularFeature(FName("SourceControl"), Provider.Get());
}

void FUnrealGitModule::ShutdownModule()
{
	if (Provider)
	{
		IModularFeatures::Get().UnregisterModularFeature(FName("SourceControl"), Provider.Get());
		Provider.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealGitModule, UnrealGit)
