// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Git/Parsers/GitLogParser.h"

#include "UnrealGit/Private/UnrealGit/Git/Parsers/GitParsingUtils.h"

bool FGitLogParser::ParseDelimited(const FString& StdOutText, TArray<FGitRevisionInfo>& OutRevisions, FString& OutError)
{
	OutRevisions.Reset();
	OutError.Reset();

	const TCHAR RecordSep = 0x001E;
	const TCHAR FieldSep = 0x001F;

	TArray<FString> Records;
	StdOutText.ParseIntoArray(Records, &RecordSep, true);

	for (const FString& Record : Records)
	{
		if (Record.IsEmpty())
		{
			continue;
		}

		TArray<FString> Fields;
		Record.ParseIntoArray(Fields, &FieldSep, false);

		if (Fields.Num() < 5)
		{
			OutError = TEXT("git log output record did not contain required fields.");
			return false;
		}

		FGitRevisionInfo Revision;
		Revision.CommitId = Fields[0];
		Revision.AuthorName = Fields[1];
		Revision.AuthorEmail = Fields[2];

		FDateTime ParsedDateUtc;
		if (UnrealGit::Parsing::ParseIso8601Utc(Fields[3], ParsedDateUtc))
		{
			Revision.AuthorDateUtc = ParsedDateUtc;
		}

		Revision.Subject = Fields[4];
		if (Fields.Num() >= 6)
		{
			Revision.Body = Fields[5];
		}

		OutRevisions.Add(MoveTemp(Revision));
	}

	return true;
}

