// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Git/Parsers/GitVersionParser.h"

static FString NormalizeOneLine(const FString& In)
{
	FString Out = In;
	Out.ReplaceInline(TEXT("\r"), TEXT(""));
	Out.ReplaceInline(TEXT("\n"), TEXT(" "));
	Out.TrimStartAndEndInline();
	return Out;
}

bool FGitVersionParser::ParseGitVersion(const FString& StdOutText, FString& OutVersionText)
{
	const FString Line = NormalizeOneLine(StdOutText);
	if (!Line.StartsWith(TEXT("git version ")))
	{
		return false;
	}

	OutVersionText = Line.Mid(FCString::Strlen(TEXT("git version ")));
	OutVersionText.TrimStartAndEndInline();
	return !OutVersionText.IsEmpty();
}

bool FGitVersionParser::ParseGitLfsVersion(const FString& StdOutText, FString& OutVersionText)
{
	const FString Line = NormalizeOneLine(StdOutText);
	int32 SlashIndex = INDEX_NONE;
	if (!Line.FindChar(TEXT('/'), SlashIndex))
	{
		return false;
	}

	const FString Prefix = Line.Left(SlashIndex);
	if (!Prefix.Contains(TEXT("git-lfs")))
	{
		return false;
	}

	int32 SpaceIndex = INDEX_NONE;
	const FString Remainder = Line.Mid(SlashIndex + 1);
	if (Remainder.FindChar(TEXT(' '), SpaceIndex))
	{
		OutVersionText = Remainder.Left(SpaceIndex).TrimStartAndEnd();
	}
	else
	{
		OutVersionText = Remainder.TrimStartAndEnd();
	}

	return !OutVersionText.IsEmpty();
}

