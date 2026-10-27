// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/Git/SystemGitProcessRunner.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"
#include "ISourceControlModule.h"

#include "UnrealGit.h"
#include "UnrealGit/Private/UnrealGit/Git/Parsers/GitParsingUtils.h"

FSystemGitProcessRunner::FSystemGitProcessRunner(FString InGitExecutablePath)
	: GitExecutablePath(MoveTemp(InGitExecutablePath))
{
	if (GitExecutablePath.IsEmpty())
	{
		GitExecutablePath = TEXT("git");
	}
}

static FString QuoteArgForCreateProc(const FString& Arg)
{
	if (Arg.IsEmpty())
	{
		return TEXT("\"\"");
	}

	const bool bNeedsQuotes = Arg.Contains(TEXT(" ")) || Arg.Contains(TEXT("\t")) || Arg.Contains(TEXT("\n")) || Arg.Contains(TEXT("\""));
	if (!bNeedsQuotes)
	{
		return Arg;
	}

	FString Escaped = Arg.Replace(TEXT("\""), TEXT("\\\""));
	return FString::Printf(TEXT("\"%s\""), *Escaped);
}

FString FSystemGitProcessRunner::BuildCommandLine(const TArray<FString>& Arguments)
{
	FString CommandLine;
	for (const FString& Arg : Arguments)
	{
		if (!CommandLine.IsEmpty())
		{
			CommandLine.AppendChar(' ');
		}
		CommandLine.Append(QuoteArgForCreateProc(Arg));
	}
	return CommandLine;
}

FGitProcessResult FSystemGitProcessRunner::RunProcessOnce(const FString& Params, const FGitProcessRequest& Request, double TimeoutSeconds, const FString& GitExecutablePath)
{
	FGitProcessResult Result;
	const double StartSeconds = FPlatformTime::Seconds();

	void* StdOutReadPipe = nullptr;
	void* StdOutWritePipe = nullptr;
	void* StdErrReadPipe = nullptr;
	void* StdErrWritePipe = nullptr;

	FPlatformProcess::CreatePipe(StdOutReadPipe, StdOutWritePipe);
	FPlatformProcess::CreatePipe(StdErrReadPipe, StdErrWritePipe);

	ON_SCOPE_EXIT
	{
		FPlatformProcess::ClosePipe(StdOutReadPipe, StdOutWritePipe);
		FPlatformProcess::ClosePipe(StdErrReadPipe, StdErrWritePipe);
	};

	uint32 ProcessId = 0;

	FProcHandle Handle = FPlatformProcess::CreateProc(
		*GitExecutablePath,
		*Params,
		/*bLaunchDetached*/ false,
		/*bLaunchHidden*/ true,
		/*bLaunchReallyHidden*/ true,
		&ProcessId,
		0,
		Request.WorkingDirectory.IsEmpty() ? nullptr : *Request.WorkingDirectory,
		StdOutWritePipe,
		nullptr,
		StdErrWritePipe);

	if (!Handle.IsValid())
	{
		Result.ExitCode = -1;
		const FString Error = FString::Printf(TEXT("Failed to launch git process: %s %s (workdir: %s)"), *GitExecutablePath, *Params, Request.WorkingDirectory.IsEmpty() ? TEXT("(null)") : *Request.WorkingDirectory);
		UE_LOG(LogUnrealGit, Error, TEXT("UnrealGit: %s"), *Error);
		FTCHARToUTF8 Utf8(*Error);
		Result.StdErr.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		Result.Duration = FTimespan::FromSeconds(FPlatformTime::Seconds() - StartSeconds);
		return Result;
	}

	ON_SCOPE_EXIT
	{
		FPlatformProcess::CloseProc(Handle);
	};

	auto ReadAllAvailable = [](void* ReadPipe, TArray<uint8>& OutBytes)
	{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
		TArray<uint8> Chunk;
		while (FPlatformProcess::ReadPipeToArray(ReadPipe, Chunk))
		{
			OutBytes.Append(Chunk);
		}
#else
		const FString Text = FPlatformProcess::ReadPipe(ReadPipe);
		FTCHARToUTF8 Utf8(*Text);
		OutBytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
#endif
	};

	while (FPlatformProcess::IsProcRunning(Handle))
	{
		ReadAllAvailable(StdOutReadPipe, Result.StdOut);
		ReadAllAvailable(StdErrReadPipe, Result.StdErr);

		if (TimeoutSeconds > 0.0 && (FPlatformTime::Seconds() - StartSeconds) > TimeoutSeconds)
		{
			UE_LOG(LogUnrealGit, Warning, TEXT("UnrealGit: Process timed out after %f seconds: %s %s"), TimeoutSeconds, *GitExecutablePath, *Params);
			Result.bWasCanceled = true;
			FPlatformProcess::TerminateProc(Handle, true);
			break;
		}

		FPlatformProcess::Sleep(0.01f);
	}

	// Final drain after exit.
	ReadAllAvailable(StdOutReadPipe, Result.StdOut);
	ReadAllAvailable(StdErrReadPipe, Result.StdErr);

	int32 ExitCode = 0;
	if (!FPlatformProcess::GetProcReturnCode(Handle, &ExitCode))
	{
		ExitCode = Result.bWasCanceled ? -2 : -1;
	}
	Result.ExitCode = ExitCode;

	Result.Duration = FTimespan::FromSeconds(FPlatformTime::Seconds() - StartSeconds);
	return Result;
}

void FSystemGitProcessRunner::EnsureStaleIndexLockRemoved(const FString& WorkingDirectory)
{
	IFileManager::Get().Delete(*FPaths::Combine(WorkingDirectory, TEXT(".git"), TEXT("index.lock")), false, true, true);
}

FGitProcessResult FSystemGitProcessRunner::Run(const FGitProcessRequest& Request)
{
	check(!IsInGameThread());

	FGitProcessResult Result;
	const double StartSeconds = FPlatformTime::Seconds();

	// If RepoRoot is set, automatically prepend "-C <RepoRoot>" to arguments
	TArray<FString> FinalArguments = Request.Arguments;
	if (!Request.RepoRoot.IsEmpty())
	{
		FinalArguments.Insert(TEXT("-C"), 0);
		FinalArguments.Insert(Request.RepoRoot, 1);

		// Clean up any stale index.lock file to prevent "Another git process" errors
		const FString IndexLockPath = FPaths::Combine(Request.RepoRoot, TEXT(".git"), TEXT("index.lock"));
		if (IFileManager::Get().FileExists(*IndexLockPath))
		{
			UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UnrealGit: Removing stale index.lock file"));
			IFileManager::Get().Delete(*IndexLockPath, false, true, true);
		}
	}

	const FString Params = BuildCommandLine(FinalArguments);

	const double TimeoutSeconds = Request.Timeout.IsSet() ? Request.Timeout->GetTotalSeconds() : FSystemGitProcessRunner::DefaultTimeoutSeconds;

	UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UnrealGit: Running git: executable='%s', args='%s', workdir='%s'"),
		*GitExecutablePath, *Params, Request.WorkingDirectory.IsEmpty() ? TEXT("(null)") : *Request.WorkingDirectory);

	Result = RunProcessOnce(Params, Request, TimeoutSeconds, GitExecutablePath);

	// Detect launch failure handled inside helper.
	const FString StdErrText = UnrealGit::Parsing::Utf8BytesToString(Result.StdErr);
	if (Result.ExitCode == -1 && StdErrText.Contains(TEXT("Failed to launch git process")))
	{
		Result.Duration = FTimespan::FromSeconds(FPlatformTime::Seconds() - StartSeconds);
		return Result;
	}

	if (Result.ExitCode == 128 && StdErrText.Contains(TEXT("index.lock")) && !Request.RepoRoot.IsEmpty())
	{
		UE_LOG(LogUnrealGit, Verbose, TEXT("UnrealGit: index.lock conflict detected, retrying after brief delay..."));
		FPlatformProcess::Sleep(0.1f);

		EnsureStaleIndexLockRemoved(Request.RepoRoot);

		FGitProcessResult RetryResult = RunProcessOnce(Params, Request, 0.0, GitExecutablePath);

		const FString RetryStdErrText = UnrealGit::Parsing::Utf8BytesToString(RetryResult.StdErr);
		const bool bRetryLaunchFailed = RetryResult.ExitCode == -1 && RetryStdErrText.Contains(TEXT("Failed to launch git process"));

		if (!bRetryLaunchFailed)
		{
			// Accumulate retry output onto the original result, mirroring the previous in-place drain.
			Result.StdOut.Append(RetryResult.StdOut);
			Result.StdErr.Append(RetryResult.StdErr);
			if (RetryResult.bWasCanceled)
			{
				Result.bWasCanceled = true;
			}
			Result.ExitCode = RetryResult.ExitCode;
		}
	}

	Result.Duration = FTimespan::FromSeconds(FPlatformTime::Seconds() - StartSeconds);
	UE_LOG(LogUnrealGit, VeryVerbose, TEXT("UnrealGit: git completed: exit=%d, duration=%.3fs"), Result.ExitCode, Result.Duration.GetTotalSeconds());
	return Result;
}