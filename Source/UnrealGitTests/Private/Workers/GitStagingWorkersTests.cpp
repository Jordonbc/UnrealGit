// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "SourceControlOperations.h"
#include "UnrealGit/Settings/UnrealGitProviderSettings.h"
#include "Mocks/FMockGitProcessRunner.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitDeleteWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitCopyWorker.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/Workers/GitResolveWorker.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealGitDeleteWorkerTest, "UnrealGit.Workers.Delete", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitDeleteWorkerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString RepoRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	// Single file.
	{
		TSharedRef<FMockGitProcessRunner, ESPMode::ThreadSafe> Runner = MakeShared<FMockGitProcessRunner, ESPMode::ThreadSafe>();

		FMockGitProcessRunner::FExpectation Exp;
		Exp.Result.ExitCode = 0;
		Runner->Enqueue(Exp);

		TSharedRef<FDelete, ESPMode::ThreadSafe> Op = ISourceControlOperation::Create<FDelete>();

		FGitDeleteWorker Worker;
		FUnrealGitWorkerOutput Output;
		Worker.Execute(Runner, FUnrealGitProviderSettings(), RepoRoot, false, Op, { FPaths::ConvertRelativePathToFull(RepoRoot / TEXT("Content/Test.uasset")) }, RepoRoot, Output);

		TestTrue(TEXT("Delete single file success"), Output.bSuccess);
		TestEqual(TEXT("Delete single file request count"), Runner->GetRequests().Num(), 1);
		TestEqual(TEXT("Delete single file command"), Runner->GetRequests()[0].Arguments[0], FString(TEXT("rm")));
		TestTrue(TEXT("Delete single file has --ignore-unmatch"), Runner->GetRequests()[0].Arguments.Contains(FString(TEXT("--ignore-unmatch"))));
		const TArray<FString>& Args = Runner->GetRequests()[0].Arguments;
		TestEqual(TEXT("Delete single file last arg"), Args[Args.Num() - 1], FString(TEXT("Content/Test.uasset")));
	}

	// Batching: 60 files -> 2 requests.
	{
		TSharedRef<FMockGitProcessRunner, ESPMode::ThreadSafe> Runner = MakeShared<FMockGitProcessRunner, ESPMode::ThreadSafe>();

		for (int32 i = 0; i < 2; ++i)
		{
			FMockGitProcessRunner::FExpectation Exp;
			Exp.Result.ExitCode = 0;
			Runner->Enqueue(Exp);
		}

		TArray<FString> Files;
		for (int32 i = 0; i < 60; ++i)
		{
			Files.Add(FPaths::ConvertRelativePathToFull(RepoRoot / FString::Printf(TEXT("Content/Test%d.uasset"), i)));
		}

		TSharedRef<FDelete, ESPMode::ThreadSafe> Op = ISourceControlOperation::Create<FDelete>();

		FGitDeleteWorker Worker;
		FUnrealGitWorkerOutput Output;
		Worker.Execute(Runner, FUnrealGitProviderSettings(), RepoRoot, false, Op, Files, RepoRoot, Output);

		TestTrue(TEXT("Delete batch success"), Output.bSuccess);
		TestEqual(TEXT("Delete batch request count"), Runner->GetRequests().Num(), 2);
		for (int32 i = 0; i < 2; ++i)
		{
			TestEqual(TEXT("Delete batch command"), Runner->GetRequests()[i].Arguments[0], FString(TEXT("rm")));
			TestTrue(TEXT("Delete batch has --ignore-unmatch"), Runner->GetRequests()[i].Arguments.Contains(FString(TEXT("--ignore-unmatch"))));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealGitCopyWorkerTest, "UnrealGit.Workers.Copy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitCopyWorkerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString RepoRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	// Single file.
	{
		TSharedRef<FMockGitProcessRunner, ESPMode::ThreadSafe> Runner = MakeShared<FMockGitProcessRunner, ESPMode::ThreadSafe>();

		FMockGitProcessRunner::FExpectation Exp;
		Exp.Result.ExitCode = 0;
		Runner->Enqueue(Exp);

		TSharedRef<FCopy, ESPMode::ThreadSafe> Op = ISourceControlOperation::Create<FCopy>();

		FGitCopyWorker Worker;
		FUnrealGitWorkerOutput Output;
		Worker.Execute(Runner, FUnrealGitProviderSettings(), RepoRoot, false, Op, { FPaths::ConvertRelativePathToFull(RepoRoot / TEXT("Content/Test.uasset")) }, RepoRoot, Output);

		TestTrue(TEXT("Copy single file success"), Output.bSuccess);
		TestEqual(TEXT("Copy single file request count"), Runner->GetRequests().Num(), 1);
		TestEqual(TEXT("Copy single file command"), Runner->GetRequests()[0].Arguments[0], FString(TEXT("add")));
		const TArray<FString>& Args = Runner->GetRequests()[0].Arguments;
		TestEqual(TEXT("Copy single file last arg"), Args[Args.Num() - 1], FString(TEXT("Content/Test.uasset")));
	}

	// Batching: 60 files -> 2 requests.
	{
		TSharedRef<FMockGitProcessRunner, ESPMode::ThreadSafe> Runner = MakeShared<FMockGitProcessRunner, ESPMode::ThreadSafe>();

		for (int32 i = 0; i < 2; ++i)
		{
			FMockGitProcessRunner::FExpectation Exp;
			Exp.Result.ExitCode = 0;
			Runner->Enqueue(Exp);
		}

		TArray<FString> Files;
		for (int32 i = 0; i < 60; ++i)
		{
			Files.Add(FPaths::ConvertRelativePathToFull(RepoRoot / FString::Printf(TEXT("Content/Test%d.uasset"), i)));
		}

		TSharedRef<FCopy, ESPMode::ThreadSafe> Op = ISourceControlOperation::Create<FCopy>();

		FGitCopyWorker Worker;
		FUnrealGitWorkerOutput Output;
		Worker.Execute(Runner, FUnrealGitProviderSettings(), RepoRoot, false, Op, Files, RepoRoot, Output);

		TestTrue(TEXT("Copy batch success"), Output.bSuccess);
		TestEqual(TEXT("Copy batch request count"), Runner->GetRequests().Num(), 2);
		for (int32 i = 0; i < 2; ++i)
		{
			TestEqual(TEXT("Copy batch command"), Runner->GetRequests()[i].Arguments[0], FString(TEXT("add")));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealGitResolveWorkerTest, "UnrealGit.Workers.Resolve", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitResolveWorkerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString RepoRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	// Single file.
	{
		TSharedRef<FMockGitProcessRunner, ESPMode::ThreadSafe> Runner = MakeShared<FMockGitProcessRunner, ESPMode::ThreadSafe>();

		FMockGitProcessRunner::FExpectation Exp;
		Exp.Result.ExitCode = 0;
		Runner->Enqueue(Exp);

		TSharedRef<FResolve, ESPMode::ThreadSafe> Op = ISourceControlOperation::Create<FResolve>();

		FGitResolveWorker Worker;
		FUnrealGitWorkerOutput Output;
		Worker.Execute(Runner, FUnrealGitProviderSettings(), RepoRoot, false, Op, { FPaths::ConvertRelativePathToFull(RepoRoot / TEXT("Content/Test.uasset")) }, RepoRoot, Output);

		TestTrue(TEXT("Resolve single file success"), Output.bSuccess);
		TestEqual(TEXT("Resolve single file request count"), Runner->GetRequests().Num(), 1);
		TestEqual(TEXT("Resolve single file command"), Runner->GetRequests()[0].Arguments[0], FString(TEXT("add")));
		const TArray<FString>& Args = Runner->GetRequests()[0].Arguments;
		TestEqual(TEXT("Resolve single file last arg"), Args[Args.Num() - 1], FString(TEXT("Content/Test.uasset")));
	}

	// Batching: 60 files -> 2 requests.
	{
		TSharedRef<FMockGitProcessRunner, ESPMode::ThreadSafe> Runner = MakeShared<FMockGitProcessRunner, ESPMode::ThreadSafe>();

		for (int32 i = 0; i < 2; ++i)
		{
			FMockGitProcessRunner::FExpectation Exp;
			Exp.Result.ExitCode = 0;
			Runner->Enqueue(Exp);
		}

		TArray<FString> Files;
		for (int32 i = 0; i < 60; ++i)
		{
			Files.Add(FPaths::ConvertRelativePathToFull(RepoRoot / FString::Printf(TEXT("Content/Test%d.uasset"), i)));
		}

		TSharedRef<FResolve, ESPMode::ThreadSafe> Op = ISourceControlOperation::Create<FResolve>();

		FGitResolveWorker Worker;
		FUnrealGitWorkerOutput Output;
		Worker.Execute(Runner, FUnrealGitProviderSettings(), RepoRoot, false, Op, Files, RepoRoot, Output);

		TestTrue(TEXT("Resolve batch success"), Output.bSuccess);
		TestEqual(TEXT("Resolve batch request count"), Runner->GetRequests().Num(), 2);
		for (int32 i = 0; i < 2; ++i)
		{
			TestEqual(TEXT("Resolve batch command"), Runner->GetRequests()[i].Arguments[0], FString(TEXT("add")));
		}
	}

	return true;
}