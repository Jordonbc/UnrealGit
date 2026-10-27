// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "UnrealGit/Git/Parsers/GitFileHistoryParser.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealGitFileHistoryParserTest, "UnrealGit.Parsers.FileHistoryRename", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitFileHistoryParserTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TArray<uint8> Bytes;
	const uint8 RS = 0x1E;
	const TCHAR US = 0x001F;

	auto AppendUtf8 = [&Bytes](const FString& S)
	{
		FTCHARToUTF8 Utf8(*S);
		Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	};

	Bytes.Add(RS);
	AppendUtf8(FString::Printf(TEXT("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa%cAlice%cAlice@Example.com%c2025-01-01T12:00:00Z%cRename"), US, US, US, US));
	Bytes.Add('\n');
	AppendUtf8(TEXT("R100\tContent/Old.uasset"));
	Bytes.Add(0);
	AppendUtf8(TEXT("Content/New.uasset"));
	Bytes.Add(0);

	Bytes.Add(RS);
	AppendUtf8(FString::Printf(TEXT("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb%cBob%cBob@Example.com%c2024-12-31T11:00:00Z%cEdit"), US, US, US, US));
	Bytes.Add('\n');
	AppendUtf8(TEXT("M\tContent/Old.uasset"));
	Bytes.Add(0);

	TArray<FGitFileRevision> History;
	FString Error;
	TestTrue(TEXT("Parse succeeds"), FGitFileHistoryParser::Parse(Bytes, TEXT("Content/New.uasset"), History, Error));
	TestEqual(TEXT("History count"), History.Num(), 2);
	TestEqual(TEXT("Newest path"), History[0].RepoRelativePathAtRevision, FString(TEXT("Content/New.uasset")));
	TestEqual(TEXT("Older path"), History[1].RepoRelativePathAtRevision, FString(TEXT("Content/Old.uasset")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealGitFileHistoryParserActionsTest, "UnrealGit.Parsers.FileHistoryActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitFileHistoryParserActionsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TArray<uint8> Bytes;
	const uint8 RS = 0x1E;
	const TCHAR US = 0x001F;

	auto AppendUtf8 = [&Bytes](const FString& S)
	{
		FTCHARToUTF8 Utf8(*S);
		Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	};

	// Newest: modified
	Bytes.Add(RS);
	AppendUtf8(FString::Printf(TEXT("%s%c%s%c%s%c%s%c%s"), TEXT("c1"), US, TEXT("Author"), US, TEXT("a@b"), US, TEXT("2025-01-03T12:00:00Z"), US, TEXT("Modify")));
	Bytes.Add('\n');
	AppendUtf8(TEXT("M\tContent/New.uasset"));
	Bytes.Add(0);

	// Middle: rename (branch)
	Bytes.Add(RS);
	AppendUtf8(FString::Printf(TEXT("%s%c%s%c%s%c%s%c%s"), TEXT("c2"), US, TEXT("Author"), US, TEXT("a@b"), US, TEXT("2025-01-02T12:00:00Z"), US, TEXT("Rename")));
	Bytes.Add('\n');
	AppendUtf8(TEXT("R100\tContent/Old.uasset"));
	Bytes.Add(0);
	AppendUtf8(TEXT("Content/New.uasset"));
	Bytes.Add(0);

	// Oldest: add
	Bytes.Add(RS);
	AppendUtf8(FString::Printf(TEXT("%s%c%s%c%s%c%s%c%s"), TEXT("c3"), US, TEXT("Author"), US, TEXT("a@b"), US, TEXT("2025-01-01T12:00:00Z"), US, TEXT("Add")));
	Bytes.Add('\n');
	AppendUtf8(TEXT("A\tContent/Old.uasset"));
	Bytes.Add(0);

	TArray<FGitFileRevision> History;
	FString Error;
	TestTrue(TEXT("Parse succeeds"), FGitFileHistoryParser::Parse(Bytes, TEXT("Content/New.uasset"), History, Error));
	TestEqual(TEXT("History count"), History.Num(), 3);

	TestEqual(TEXT("Newest action"), History[0].Revision.Action, FString(TEXT("modified")));
	TestEqual(TEXT("Middle action"), History[1].Revision.Action, FString(TEXT("branch")));
	TestEqual(TEXT("Oldest action"), History[2].Revision.Action, FString(TEXT("add")));

	TestEqual(TEXT("Newest revision number"), History[0].Revision.RevisionNumber, 3);
	TestEqual(TEXT("Middle revision number"), History[1].Revision.RevisionNumber, 2);
	TestEqual(TEXT("Oldest revision number"), History[2].Revision.RevisionNumber, 1);

	TestEqual(TEXT("Newest path"), History[0].RepoRelativePathAtRevision, FString(TEXT("Content/New.uasset")));
	TestEqual(TEXT("Middle path"), History[1].RepoRelativePathAtRevision, FString(TEXT("Content/New.uasset")));
	TestEqual(TEXT("Oldest path"), History[2].RepoRelativePathAtRevision, FString(TEXT("Content/Old.uasset")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealGitFileHistoryParserSizeMappingTest, "UnrealGit.Parsers.FileHistorySizeMapping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitFileHistoryParserSizeMappingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TArray<FGitFileRevision> History;
	FGitFileRevision R1;
	R1.Revision.CommitId = TEXT("c1");
	R1.RepoRelativePathAtRevision = TEXT("p1");
	History.Add(R1);
	FGitFileRevision R2;
	R2.Revision.CommitId = TEXT("c2");
	R2.RepoRelativePathAtRevision = TEXT("p2");
	History.Add(R2);
	FGitFileRevision R3;
	R3.Revision.CommitId = TEXT("c3");
	R3.RepoRelativePathAtRevision = TEXT("p3");
	History.Add(R3);

	const FString Response = TEXT("1111111111111111111111111111111111111111 blob 10\n")
		TEXT("2222222222222222222222222222222222222222 missing\n")
		TEXT("3333333333333333333333333333333333333333 ambiguous\n");

	FGitFileHistoryParser::ParseCatFileBatchCheckOutput(Response, History);

	TestEqual(TEXT("First size"), History[0].Revision.FileSize, 10);
	TestEqual(TEXT("Second size"), History[1].Revision.FileSize, 0);
	TestEqual(TEXT("Third size"), History[2].Revision.FileSize, 0);

	// empty response line leaves 0
	TArray<FGitFileRevision> History2;
	FGitFileRevision A;
	A.Revision.CommitId = TEXT("c");
	A.RepoRelativePathAtRevision = TEXT("p");
	History2.Add(A);
	FGitFileHistoryParser::ParseCatFileBatchCheck(TEXT("\n"), History2);
	TestEqual(TEXT("Empty line size"), History2[0].Revision.FileSize, 0);

	return true;
}

