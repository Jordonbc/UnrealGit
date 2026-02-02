// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Git/GitRevisionMaterializer.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "UnrealGit/Git/GitProcessTypes.h"
#include "UnrealGit/Git/IGitProcessRunner.h"

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
	ShowRequest.WorkingDirectory = RepoRoot;
	ShowRequest.Arguments = { TEXT("show"), FString::Printf(TEXT("%s:%s"), *CommitId, *RelativePath) };

	const FGitProcessResult ShowResult = ProcessRunner->Run(ShowRequest);
	if (ShowResult.ExitCode != 0)
	{
		if (ShowResult.StdErr.Num() > 0)
		{
			FUTF8ToTCHAR Err(reinterpret_cast<const ANSICHAR*>(ShowResult.StdErr.GetData()), ShowResult.StdErr.Num());
			OutError = FString(Err.Length(), Err.Get());
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
