// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "UnrealGit/Git/Parsers/GitLfsLocksParser.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealGitLfsLocksParserTest, "UnrealGit.Parsers.LfsLocksJson", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitLfsLocksParserTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString Json = TEXT("{\"locks\":[{\"id\":\"1\",\"path\":\"Content/Test.uasset\",\"locked_at\":\"2025-01-01T12:00:00Z\",\"owner\":{\"name\":\"Alice\",\"email\":\"alice@example.com\"}}]}");
	TArray<FGitLfsLock> Locks;
	FString Error;
	TestTrue(TEXT("Parse succeeds"), FGitLfsLocksParser::ParseJson(Json, Locks, Error));
	TestEqual(TEXT("Lock count"), Locks.Num(), 1);
	TestEqual(TEXT("Path"), Locks[0].Path, FString(TEXT("Content/Test.uasset")));
	TestEqual(TEXT("Owner name"), Locks[0].OwnerName, FString(TEXT("Alice")));
	return true;
}

