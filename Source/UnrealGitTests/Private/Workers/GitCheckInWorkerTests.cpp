// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "SourceControlOperations.h"
#include "UnrealGit/Settings/UnrealGitProviderSettings.h"
#include "Mocks/FMockGitProcessRunner.h"
#include "UnrealGit/SourceControl/Workers/GitCheckInWorker.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealGitCheckInWorkerTest, "UnrealGit.Workers.CheckInSequencing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitCheckInWorkerTest::RunTest(const FString&)
{
	TSharedRef<FMockGitProcessRunner, ESPMode::ThreadSafe> Runner = MakeShared<FMockGitProcessRunner, ESPMode::ThreadSafe>();

	FMockGitProcessRunner::FExpectation AddExp;
	AddExp.Result.ExitCode = 0;
	Runner->Enqueue(AddExp);

	FMockGitProcessRunner::FExpectation CommitExp;
	CommitExp.Result.ExitCode = 0;
	Runner->Enqueue(CommitExp);

	FUnrealGitProviderSettings Settings;
	Settings.bAutoPushAfterSubmit = false;

	TSharedRef<FCheckIn, ESPMode::ThreadSafe> Op = ISourceControlOperation::Create<FCheckIn>();
	Op->SetDescription(FText::FromString(TEXT("Test commit\nBody")));

	const FString RepoRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	FGitCheckInWorker Worker;
	FUnrealGitWorkerOutput Output;
	Worker.Execute(Runner, Settings, RepoRoot, false, Op, { FPaths::ConvertRelativePathToFull(RepoRoot / TEXT("Content/Test.uasset")) }, RepoRoot, Output);

	TestTrue(TEXT("Worker success"), Output.bSuccess);
	TestTrue(TEXT("Runner called twice"), Runner->GetRequests().Num() == 2);
	TestEqual(TEXT("First command is add"), Runner->GetRequests()[0].Arguments[0], FString(TEXT("add")));
	TestEqual(TEXT("Second command is commit"), Runner->GetRequests()[1].Arguments[0], FString(TEXT("commit")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealGitCheckInWorkerBatch60Test, "UnrealGit.Workers.CheckInBatch60", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitCheckInWorkerBatch60Test::RunTest(const FString&)
{
	TSharedRef<FMockGitProcessRunner, ESPMode::ThreadSafe> Runner = MakeShared<FMockGitProcessRunner, ESPMode::ThreadSafe>();

	for (int32 i = 0; i < 4; ++i)
	{
		FMockGitProcessRunner::FExpectation Exp;
		Exp.Result.ExitCode = 0;
		Runner->Enqueue(Exp);
	}

	FUnrealGitProviderSettings Settings;
	Settings.bAutoPushAfterSubmit = false;

	TSharedRef<FCheckIn, ESPMode::ThreadSafe> Op = ISourceControlOperation::Create<FCheckIn>();
	Op->SetDescription(FText::FromString(TEXT("Batch commit")));

	const FString RepoRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	TArray<FString> Files;
	for (int32 i = 0; i < 60; ++i)
	{
		Files.Add(FPaths::ConvertRelativePathToFull(RepoRoot / FString::Printf(TEXT("Content/File%d.uasset"), i)));
	}

	FGitCheckInWorker Worker;
	FUnrealGitWorkerOutput Output;
	Worker.Execute(Runner, Settings, RepoRoot, false, Op, Files, RepoRoot, Output);

	TestTrue(TEXT("Worker success"), Output.bSuccess);
	TestEqual(TEXT("Total requests"), Runner->GetRequests().Num(), 4);

	int32 AddCount = 0;
	int32 CommitCount = 0;
	int32 AmendCount = 0;
	for (const FGitProcessRequest& Req : Runner->GetRequests())
	{
		if (Req.Arguments.Num() > 0 && Req.Arguments[0] == TEXT("add"))
		{
			++AddCount;
		}
		else if (Req.Arguments.Num() > 0 && Req.Arguments[0] == TEXT("commit"))
		{
			++CommitCount;
			if (Req.Arguments.Contains(TEXT("--amend")))
			{
				++AmendCount;
			}
		}
	}

	TestEqual(TEXT("Add batches"), AddCount, 2);
	TestEqual(TEXT("Commit invocations"), CommitCount, 2);
	TestEqual(TEXT("Amend commits"), AmendCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealGitCheckInWorkerBatch120Test, "UnrealGit.Workers.CheckInBatch120", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitCheckInWorkerBatch120Test::RunTest(const FString&)
{
	TSharedRef<FMockGitProcessRunner, ESPMode::ThreadSafe> Runner = MakeShared<FMockGitProcessRunner, ESPMode::ThreadSafe>();

	for (int32 i = 0; i < 6; ++i)
	{
		FMockGitProcessRunner::FExpectation Exp;
		Exp.Result.ExitCode = 0;
		Runner->Enqueue(Exp);
	}

	FUnrealGitProviderSettings Settings;
	Settings.bAutoPushAfterSubmit = false;

	TSharedRef<FCheckIn, ESPMode::ThreadSafe> Op = ISourceControlOperation::Create<FCheckIn>();
	Op->SetDescription(FText::FromString(TEXT("Batch commit")));

	const FString RepoRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	TArray<FString> Files;
	for (int32 i = 0; i < 120; ++i)
	{
		Files.Add(FPaths::ConvertRelativePathToFull(RepoRoot / FString::Printf(TEXT("Content/File%d.uasset"), i)));
	}

	FGitCheckInWorker Worker;
	FUnrealGitWorkerOutput Output;
	Worker.Execute(Runner, Settings, RepoRoot, false, Op, Files, RepoRoot, Output);

	TestTrue(TEXT("Worker success"), Output.bSuccess);
	TestEqual(TEXT("Total requests"), Runner->GetRequests().Num(), 6);

	int32 AddCount = 0;
	int32 CommitCount = 0;
	int32 AmendCount = 0;
	for (const FGitProcessRequest& Req : Runner->GetRequests())
	{
		if (Req.Arguments.Num() > 0 && Req.Arguments[0] == TEXT("add"))
		{
			++AddCount;
		}
		else if (Req.Arguments.Num() > 0 && Req.Arguments[0] == TEXT("commit"))
		{
			++CommitCount;
			if (Req.Arguments.Contains(TEXT("--amend")))
			{
				++AmendCount;
			}
		}
	}

	TestEqual(TEXT("Add batches"), AddCount, 3);
	TestEqual(TEXT("Commit invocations"), CommitCount, 3);
	TestEqual(TEXT("Amend commits"), AmendCount, 2);
	return true;
}
