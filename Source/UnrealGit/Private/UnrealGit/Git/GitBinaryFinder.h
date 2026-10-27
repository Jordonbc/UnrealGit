// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Discovers the git executable by probing well-known install locations.
 *
 * Responsibility:
 * - Filesystem-only lookup; never spawns a process or logs.
 */
namespace UnrealGit::Git
{
	/** Returns the first existing executable from the platform candidate list, or "git" when none matched. @return Resolved git executable path. */
	FString FindGitBinary();

	/**
	 * Returns true if Path is usable as a git executable (filesystem check only).
	 *
	 * @param Path Candidate executable path.
	 * @return True when the path exists and is executable.
	 */
	bool IsValidGitBinary(const FString& Path);
}