// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit.h"

#include "HAL/FileManager.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitPathUtils.h"
#include "UnrealGit/Git/IGitProcessRunner.h"
#include "UnrealGit/SourceControl/Workers/UnrealGitSourceControlWorkers.h"

namespace UnrealGit::Workers
{
	inline bool TryFixCorruptedIndex(
		const TSharedPtr<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
		const FString& RepoRoot)
	{
		const FString IndexPath = FPaths::Combine(RepoRoot, TEXT(".git"), TEXT("index"));
		if (!IFileManager::Get().FileExists(*IndexPath))
		{
			return false;
		}

		UE_LOG(LogUnrealGit, Warning, TEXT("Detected corrupted git index, attempting to rebuild..."));

		IFileManager::Get().Delete(*IndexPath, false, true, true);

		FGitProcessRequest Request;
		Request.RepoRoot = RepoRoot;
		Request.Arguments = { TEXT("reset") };

		const FGitProcessResult Result = ProcessRunner->Run(Request);
		if (Result.ExitCode == 0)
		{
			UE_LOG(LogUnrealGit, Log, TEXT("Successfully rebuilt git index"));
			return true;
		}

		UE_LOG(LogUnrealGit, Error, TEXT("Failed to rebuild git index: %s"), *BytesToTextUtf8Lossy(Result.StdErr));
		return false;
	}
}
