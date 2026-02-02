// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Git/GitModels.h"

/**
 * Parses `git log` output formatted with US/RS delimiters.
 *
 * This parser is deterministic and side-effect free.
 */
class FGitLogParser final
{
public:
	static bool ParseDelimited(const FString& StdOutText, TArray<FGitRevisionInfo>& OutRevisions, FString& OutError);
};

