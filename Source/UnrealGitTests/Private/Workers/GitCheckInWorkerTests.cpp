// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "SourceControlOperations.h"
#include "UnrealGit/Settings/UnrealGitProviderSettings.h"
#include "Mocks/FMockGitProcessRunner.h"
#include "UnrealGit/SourceControl/Workers/GitCheckInWorker.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealGitCheckInWorkerTest, "UnrealGit.Workers.CheckInSequencing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitCheckInWorkerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

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
