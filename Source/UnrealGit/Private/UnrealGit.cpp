// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit.h"

#include "Features/IModularFeatures.h"
#include "HAL/IConsoleManager.h"
#include "UnrealGit/Private/UnrealGit/UI/UnrealGitAssetContextMenuExtension.h"
#include "UnrealGit/SourceControl/UnrealGitSourceControlProvider.h"
#include "UnrealGit/UI/UnrealGitHistoryDiffExtension.h"

DEFINE_LOG_CATEGORY(LogUnrealGit);

#define LOCTEXT_NAMESPACE "FUnrealGitModule"

void FUnrealGitModule::StartupModule()
{
	Provider = MakeUnique<FUnrealGitSourceControlProvider>();
	IModularFeatures::Get().RegisterModularFeature(FName("SourceControl"), Provider.Get());
	FUnrealGitHistoryDiffExtension::Register();
	FUnrealGitAssetContextMenuExtension::Register();

	StatusCommand = MakeUnique<FAutoConsoleCommand>(
		TEXT("UnrealGit.Status"),
		TEXT("Print UnrealGit source control diagnostics (repo, branch, locks, cached states)."),
		FConsoleCommandDelegate::CreateLambda([this]()
		{
			if (Provider)
			{
				Provider->LogDiagnostics();
			}
		}));
}

void FUnrealGitModule::ShutdownModule()
{
	if (Provider)
	{
		FUnrealGitHistoryDiffExtension::Unregister();
		FUnrealGitAssetContextMenuExtension::Unregister();
		StatusCommand.Reset();
		IModularFeatures::Get().UnregisterModularFeature(FName("SourceControl"), Provider.Get());
		Provider.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealGitModule, UnrealGit)
