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

	inline FGitProcessRequest MakeGitRequest(const FString& RepoRoot, const TArray<FString>& Arguments)
	{
		FGitProcessRequest Request;
		Request.WorkingDirectory = FString();
		Request.Arguments = { TEXT("-C"), *RepoRoot };
		for (const FString& Arg : Arguments)
		{
			Request.Arguments.Add(Arg);
		}
		return Request;
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
		UE_LOG(LogUnrealGit, Log, TEXT("EnsureRepoRoot: Searching from directory: '%s'"), *CurrentDir);
		CurrentDir = FPaths::ConvertRelativePathToFull(CurrentDir);
		FPaths::NormalizeDirectoryName(CurrentDir);

		FGitProcessRequest RootRequest;
		RootRequest.WorkingDirectory = FString();
		RootRequest.Arguments = { TEXT("-C"), *CurrentDir, TEXT("rev-parse"), TEXT("--show-toplevel") };

		const FGitProcessResult RootResult = ProcessRunner->Run(RootRequest);
		const FString StdErr = BytesToTextUtf8Lossy(RootResult.StdErr).TrimStartAndEnd();
		const FString StdOut = BytesToTextUtf8Lossy(RootResult.StdOut);

		UE_LOG(LogUnrealGit, Log, TEXT("EnsureRepoRoot: git -C rev-parse result: exit=%d, stdout='%s', stderr='%s'"), 
			RootResult.ExitCode, *StdOut.TrimStartAndEnd(), *StdErr);

		if (RootResult.ExitCode == 0 && FGitRevParseParser::ParseShowToplevel(StdOut, InOutRepoRoot))
		{
			return true;
		}

		if (OutError)
		{
			const FString Err = StdErr.IsEmpty()
				? FString::Printf(TEXT("Git repository root could not be determined from \"%s\" (exit code: %d)."),
					*CurrentDir, RootResult.ExitCode)
				: FString::Printf(TEXT("Git repository root could not be determined from \"%s\" (exit code: %d, stderr: %s)."),
					*CurrentDir, RootResult.ExitCode, *StdErr);
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
