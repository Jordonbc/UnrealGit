// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "UnrealGit/Git/Parsers/GitLfsLocksParser.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealGitLfsLocksParserTest, "UnrealGit.Parsers.LfsLocksJson", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitLfsLocksParserTest::RunTest(const FString&)
{
	const FString Json = TEXT("{\"locks\":[{\"id\":\"1\",\"path\":\"Content/Test.uasset\",\"locked_at\":\"2025-01-01T12:00:00Z\",\"owner\":{\"name\":\"Alice\",\"email\":\"alice@example.com\"}}]}");
	TArray<FGitLfsLock> Locks;
	FString Error;
	TestTrue(TEXT("Parse succeeds"), FGitLfsLocksParser::ParseJson(Json, Locks, Error));
	TestEqual(TEXT("Lock count"), Locks.Num(), 1);
	TestEqual(TEXT("Path"), Locks[0].Path, FString(TEXT("Content/Test.uasset")));
	TestEqual(TEXT("Owner name"), Locks[0].OwnerName, FString(TEXT("Alice")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealGitLfsLocksParserArrayTest, "UnrealGit.Parsers.LfsLocksJsonArray", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitLfsLocksParserArrayTest::RunTest(const FString&)
{
	// Actual `git lfs locks --json` output: a bare array, owner without email.
	const FString Json = TEXT("[{\"id\":\"13\",\"path\":\"Content/Blueprints/BP_GraphicsSettings.uasset\",\"owner\":{\"name\":\"jordonbc\"},\"locked_at\":\"2026-10-27T11:46:21Z\"}]");
	TArray<FGitLfsLock> Locks;
	FString Error;
	TestTrue(TEXT("Parse succeeds"), FGitLfsLocksParser::ParseJson(Json, Locks, Error));
	TestEqual(TEXT("Lock count"), Locks.Num(), 1);
	TestEqual(TEXT("Path"), Locks[0].Path, FString(TEXT("Content/Blueprints/BP_GraphicsSettings.uasset")));
	TestEqual(TEXT("Owner name"), Locks[0].OwnerName, FString(TEXT("jordonbc")));
	return true;
}

