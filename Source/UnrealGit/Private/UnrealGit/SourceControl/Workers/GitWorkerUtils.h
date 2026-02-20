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

	inline TArray<FString> ParseNullDelimitedList(const TArray<uint8>& Bytes)
	{
		TArray<FString> Results;

		if (Bytes.Num() == 0)
		{
			return Results;
		}

		int32 SegmentStart = 0;
		for (int32 i = 0; i < Bytes.Num(); ++i)
		{
			if (Bytes[i] == 0)
			{
				const int32 SegmentLength = i - SegmentStart;
				if (SegmentLength > 0)
				{
					FUTF8ToTCHAR Converter(
						reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + SegmentStart),
						SegmentLength);
					Results.Add(FString(Converter.Length(), Converter.Get()));
				}
				SegmentStart = i + 1;
			}
		}

		const int32 RemainingLength = Bytes.Num() - SegmentStart;
		if (RemainingLength > 0)
		{
			FUTF8ToTCHAR Converter(
				reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + SegmentStart),
				RemainingLength);
			Results.Add(FString(Converter.Length(), Converter.Get()));
		}

		return Results;
	}

	inline FGitProcessRequest MakeGitRequest(const FString& RepoRoot, const TArray<FString>& Arguments)
	{
		FGitProcessRequest Request;
		Request.RepoRoot = RepoRoot;
		Request.WorkingDirectory = FString();
		Request.Arguments = Arguments;
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
			UE_LOG(LogUnrealGit, VeryVerbose, TEXT("TryMakeRepoRelativePath: FAIL - RepoRoot is empty. Absolute='%s'"), *AbsolutePath);
			return false;
		}

		const FString Abs = FPaths::ConvertRelativePathToFull(AbsolutePath);
		FString Rel = Abs;
		if (!FPaths::MakePathRelativeTo(Rel, *RepoRoot))
		{
			UE_LOG(LogUnrealGit, VeryVerbose, TEXT("TryMakeRepoRelativePath: FAIL - MakePathRelativeTo failed. Abs='%s', RepoRoot='%s'"), *Abs, *RepoRoot);
			return false;
		}

		FString RepoRootDirName = FPaths::GetCleanFilename(RepoRoot);
		if (!RepoRootDirName.IsEmpty() && Rel.StartsWith(RepoRootDirName + TEXT("/")))
		{
			Rel = Rel.RightChop(RepoRootDirName.Len() + 1);
		}

		if (Rel.IsEmpty() || Rel == TEXT("..") || Rel.StartsWith(TEXT("../")) || Rel.Contains(TEXT("/../")))
		{
			UE_LOG(LogUnrealGit, VeryVerbose, TEXT("TryMakeRepoRelativePath: FAIL - Validation failed. Rel='%s'"), *Rel);
			return false;
		}

		UE_LOG(LogUnrealGit, VeryVerbose, TEXT("TryMakeRepoRelativePath: SUCCESS - '%s' -> '%s'"), *Abs, *Rel);
		OutRelative = MoveTemp(Rel);
		return true;
	}
}
