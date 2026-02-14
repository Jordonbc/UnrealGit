// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit.h"

#include "Logging/LogMacros.h"
#include "UnrealGit/Git/IGitProcessRunner.h"
#include "UnrealGit/Git/Parsers/GitRevParseParser.h"

namespace UnrealGit::Workers
{
	inline FString BytesToTextUtf8Lossy(const TArray<uint8>& Bytes)
	{
		if (Bytes.Num() == 0)
		{
			return FString();
		}

		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
		return FString(Converter.Length(), Converter.Get());
	}

	inline bool EnsureRepoRoot(
		const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe>& ProcessRunner,
		const FString& WorkingDirectoryHint,
		FString& InOutRepoRoot,
		FString* OutError = nullptr)
	{
		if (!InOutRepoRoot.IsEmpty())
		{
			return true;
		}

		FString CurrentDir = WorkingDirectoryHint;
		if (CurrentDir.IsEmpty())
		{
			CurrentDir = FPaths::ProjectDir();
		}
		UE_LOG(LogUnrealGit, Log, TEXT("EnsureRepoRoot: Starting search from directory: '%s' (hint was: '%s')"), *CurrentDir, *WorkingDirectoryHint);
		CurrentDir = FPaths::ConvertRelativePathToFull(CurrentDir);
		FPaths::NormalizeDirectoryName(CurrentDir);

		int32 LastExitCode = INDEX_NONE;
		FString LastStdErr;
		FString LastAttemptDir;

		for (int32 Depth = 0; Depth < 32; ++Depth)
		{
			LastAttemptDir = CurrentDir;
			UE_LOG(LogUnrealGit, Log, TEXT("EnsureRepoRoot: Attempting git rev-parse in: '%s'"), *CurrentDir);

			FGitProcessRequest RootRequest;
			RootRequest.WorkingDirectory = CurrentDir;
			RootRequest.Arguments = { TEXT("rev-parse"), TEXT("--show-toplevel") };

			const FGitProcessResult RootResult = ProcessRunner->Run(RootRequest);
			LastExitCode = RootResult.ExitCode;
			LastStdErr = BytesToTextUtf8Lossy(RootResult.StdErr).TrimStartAndEnd();

			UE_LOG(LogUnrealGit, Log, TEXT("EnsureRepoRoot: git rev-parse result: exit=%d, stdout='%s', stderr='%s'"), 
				RootResult.ExitCode, *BytesToTextUtf8Lossy(RootResult.StdOut).TrimStartAndEnd(), *LastStdErr);

			const FString RootStdOut = BytesToTextUtf8Lossy(RootResult.StdOut);
			if (RootResult.ExitCode == 0 && FGitRevParseParser::ParseShowToplevel(RootStdOut, InOutRepoRoot))
			{
				return true;
			}

			const FString ParentDir = FPaths::GetPath(CurrentDir);
			if (ParentDir.IsEmpty() || ParentDir == CurrentDir)
			{
				break;
			}

			CurrentDir = ParentDir;
		}

		if (OutError)
		{
			const FString Err = LastStdErr.IsEmpty()
				? FString::Printf(TEXT("Git repository root could not be determined from \"%s\" (last attempt: \"%s\", exit code: %d)."),
					*WorkingDirectoryHint, *LastAttemptDir, LastExitCode)
				: FString::Printf(TEXT("Git repository root could not be determined from \"%s\" (last attempt: \"%s\", exit code: %d, stderr: %s)."),
					*WorkingDirectoryHint, *LastAttemptDir, LastExitCode, *LastStdErr);
			*OutError = Err;
		}

		return false;
	}

	inline bool TryMakeRepoRelativePath(const FString& RepoRoot, const FString& AbsolutePath, FString& OutRelative)
	{
		if (RepoRoot.IsEmpty())
		{
			return false;
		}

		const FString Abs = FPaths::ConvertRelativePathToFull(AbsolutePath);
		FString Rel = Abs;
		if (!FPaths::MakePathRelativeTo(Rel, *RepoRoot))
		{
			return false;
		}

		FPaths::MakeStandardFilename(Rel);
		OutRelative = MoveTemp(Rel);
		return true;
	}
}
