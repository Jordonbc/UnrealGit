// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "UnrealGit/Git/Parsers/GitStatusParser.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealGitStatusParserTest, "UnrealGit.Parsers.StatusPorcelainV2Z", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealGitStatusParserTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TArray<uint8> Bytes;
	auto AppendZ = [&Bytes](const FString& S)
	{
		FTCHARToUTF8 Utf8(*S);
		Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		Bytes.Add(0);
	};

	AppendZ(TEXT("# branch.head main"));
	AppendZ(TEXT("# branch.ab +1 -2"));
	AppendZ(TEXT("1 .M N... 100644 100644 100644 abcdef1 abcdef2 Content/Path With Spaces/Foo.uasset"));
	AppendZ(TEXT("? Content/Untracked/New File.txt"));
	AppendZ(TEXT("? Content/路径/文件.txt"));
	AppendZ(TEXT("! Content/Ignored/Ignore.me"));

	FGitStatusSnapshot Snapshot;
	FString Error;
	TestTrue(TEXT("Parse succeeds"), FGitStatusParser::ParsePorcelainV2Z(Bytes, Snapshot, Error));
	TestEqual(TEXT("Branch head"), Snapshot.Branch.Head, FString(TEXT("main")));
	TestTrue(TEXT("Ahead parsed"), Snapshot.Branch.Ahead.IsSet());
	TestTrue(TEXT("Behind parsed"), Snapshot.Branch.Behind.IsSet());
	TestEqual(TEXT("Files parsed"), Snapshot.Files.Num(), 4);

	const FGitFileStatus& Modified = Snapshot.Files[0];
	TestEqual(TEXT("Modified path"), Modified.RelativePath, FString(TEXT("Content/Path With Spaces/Foo.uasset")));
	TestTrue(TEXT("Unstaged"), Modified.bIsUnstaged);

	const FGitFileStatus& Untracked = Snapshot.Files[1];
	TestEqual(TEXT("Untracked state"), (uint8)Untracked.State, (uint8)EGitFileState::Untracked);

	const FGitFileStatus& UnicodeUntracked = Snapshot.Files[2];
	TestEqual(TEXT("Unicode untracked path"), UnicodeUntracked.RelativePath, FString(TEXT("Content/路径/文件.txt")));

	const FGitFileStatus& Ignored = Snapshot.Files[3];
	TestEqual(TEXT("Ignored state"), (uint8)Ignored.State, (uint8)EGitFileState::Ignored);

	return true;
}
