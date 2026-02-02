// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Git/Parsers/GitRevParseParser.h"

bool FGitRevParseParser::ParseShowToplevel(const FString& StdOutText, FString& OutRepoRoot)
{
	OutRepoRoot = StdOutText;
	OutRepoRoot.TrimStartAndEndInline();

	return !OutRepoRoot.IsEmpty();
}

