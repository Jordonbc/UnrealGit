// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

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
		FString& InOutRepoRoot)
	{
		if (!InOutRepoRoot.IsEmpty())
		{
			return true;
		}

		FGitProcessRequest RootRequest;
		RootRequest.WorkingDirectory = WorkingDirectoryHint;
		RootRequest.Arguments = { TEXT("rev-parse"), TEXT("--show-toplevel") };

		const FGitProcessResult RootResult = ProcessRunner->Run(RootRequest);
		const FString RootStdOut = BytesToTextUtf8Lossy(RootResult.StdOut);
		return RootResult.ExitCode == 0 && FGitRevParseParser::ParseShowToplevel(RootStdOut, InOutRepoRoot);
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

