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

