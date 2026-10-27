// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Git/GitModels.h"

/**
 * @class FGitLogParser
 * Parses `git log` output formatted with US/RS delimiters.
 *
 * This parser is deterministic and side-effect free.
 */
class FGitLogParser final
{
public:
	/**
	 * Parses `git log` output formatted with US/RS delimiters into revisions.
	 *
	 * @param StdOutText Raw stdout text (US/RS delimited).
	 * @param OutRevisions Parsed revisions on success.
	 * @param OutError Error message on parse failure.
	 * @return True on success, false otherwise.
	 */
	static bool ParseDelimited(const FString& StdOutText, TArray<FGitRevisionInfo>& OutRevisions, FString& OutError);
};

