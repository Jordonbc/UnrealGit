// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Git/GitRevisionMaterializer.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "UnrealGit/Git/GitProcessTypes.h"
#include "UnrealGit/Git/IGitProcessRunner.h"
#include "UnrealGit/Private/UnrealGit/Git/Parsers/GitParsingUtils.h"

FGitRevisionMaterializer::FGitRevisionMaterializer(FString InSessionRootDirectory)
	: SessionRootDirectory(MoveTemp(InSessionRootDirectory))
{
	IFileManager::Get().MakeDirectory(*SessionRootDirectory, true);
}

FGitRevisionMaterializer::~FGitRevisionMaterializer()
{
	Cleanup();
}

bool FGitRevisionMaterializer::Materialize(
	const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
	const FString& RepoRoot,
	const FString& CommitId,
	const FString& RelativePath,
	FString& OutAbsoluteFilename,
	FString& OutError)
{
	OutAbsoluteFilename.Reset();
	OutError.Reset();

	if (RepoRoot.IsEmpty() || CommitId.IsEmpty() || RelativePath.IsEmpty())
	{
		OutError = TEXT("Invalid materialization request.");
		return false;
	}

	FKey Key{ CommitId, RelativePath };
	{
		FScopeLock Scope(&Lock);
		if (const FString* Existing = MaterializedFiles.Find(Key))
		{
			OutAbsoluteFilename = *Existing;
			return true;
		}
	}

	const FString Extension = FPaths::GetExtension(RelativePath, true);
	const FString FileName = FString::Printf(TEXT("%s_%s%s"), *CommitId.Left(12), *LexToString(GetTypeHash(RelativePath)), *Extension);
	const FString OutputFile = FPaths::Combine(SessionRootDirectory, FileName);

	FGitProcessRequest ShowRequest;
	ShowRequest.RepoRoot = RepoRoot;
	ShowRequest.WorkingDirectory = FString();
	// Use cat-file --filters so LFS-managed assets are smudged to their real content.
	ShowRequest.Arguments = { TEXT("cat-file"), TEXT("--filters"), FString::Printf(TEXT("%s:%s"), *CommitId, *RelativePath) };

	FGitProcessResult ShowResult = ProcessRunner->Run(ShowRequest);
	if (ShowResult.ExitCode != 0)
	{
		// Fall back to git show for older Git versions.
		FGitProcessRequest FallbackRequest;
		FallbackRequest.RepoRoot = RepoRoot;
		FallbackRequest.WorkingDirectory = FString();
		FallbackRequest.Arguments = { TEXT("show"), FString::Printf(TEXT("%s:%s"), *CommitId, *RelativePath) };
		ShowResult = ProcessRunner->Run(FallbackRequest);
	}
	if (ShowResult.ExitCode != 0)
	{
		if (ShowResult.StdErr.Num() > 0)
		{
			OutError = UnrealGit::Parsing::Utf8BytesToString(ShowResult.StdErr);
		}
		else
		{
			OutError = TEXT("git show failed.");
		}
		return false;
	}

	if (!FFileHelper::SaveArrayToFile(ShowResult.StdOut, *OutputFile))
	{
		OutError = TEXT("Failed to write materialized revision file.");
		return false;
	}

	{
		FScopeLock Scope(&Lock);
		MaterializedFiles.Add(MoveTemp(Key), OutputFile);
	}

	OutAbsoluteFilename = OutputFile;
	return true;
}

void FGitRevisionMaterializer::Cleanup()
{
	FScopeLock Scope(&Lock);
	for (const TPair<FKey, FString>& Pair : MaterializedFiles)
	{
		IFileManager::Get().Delete(*Pair.Value, false, true, true);
	}
	MaterializedFiles.Reset();

	IFileManager::Get().DeleteDirectory(*SessionRootDirectory, false, true);
}
