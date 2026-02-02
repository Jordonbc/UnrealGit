// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealGit/Git/GitRevisionMaterializer.h"
#include "Mocks/FMockGitProcessRunner.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealGitRevisionMaterializerTest, "UnrealGit.Revision.MaterializeBinary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitRevisionMaterializerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString SessionDir = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("UnrealGitTests"), TEXT("Materialize"));
	TSharedRef<FGitRevisionMaterializer, ESPMode::ThreadSafe> Materializer = MakeShared<FGitRevisionMaterializer, ESPMode::ThreadSafe>(SessionDir);

	TSharedRef<FMockGitProcessRunner, ESPMode::ThreadSafe> Runner = MakeShared<FMockGitProcessRunner, ESPMode::ThreadSafe>();

	FMockGitProcessRunner::FExpectation Exp;
	Exp.Result.ExitCode = 0;
	Exp.Result.StdOut = { 0xDE, 0xAD, 0xBE, 0xEF };
	Runner->Enqueue(Exp);

	FString OutFile;
	FString Error;
	TestTrue(TEXT("Materialize succeeds"), Materializer->Materialize(Runner, TEXT("C:/Repo"), TEXT("deadbeef"), TEXT("Content/Test.uasset"), OutFile, Error));
	TestTrue(TEXT("Temp file created"), IFileManager::Get().FileExists(*OutFile));

	TArray<uint8> Read;
	TestTrue(TEXT("Read file"), FFileHelper::LoadFileToArray(Read, *OutFile));
	TestEqual(TEXT("Bytes match"), Read.Num(), 4);
	TestEqual(TEXT("Byte0"), Read[0], (uint8)0xDE);

	Materializer->Cleanup();
	TestFalse(TEXT("Temp file cleaned"), IFileManager::Get().FileExists(*OutFile));
	return true;
}
