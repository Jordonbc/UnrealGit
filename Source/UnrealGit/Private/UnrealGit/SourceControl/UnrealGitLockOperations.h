// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SourceControlOperations.h"

/**
 * Force unlock operation for Git LFS.
 *
 * Unlocks a lock owned by any user (requires permission on the remote). This is the
 * recovery action for stale or other-user locks; normal checkout/checkin acquire and
 * release locks implicitly.
 */
class FUnrealGitForceUnlockOperation final : public FSourceControlOperationBase
{
public:
	virtual FName GetName() const override
	{
		return "ForceUnlock";
	}

	virtual FText GetInProgressString() const override
	{
		return INVTEXT("Force unlocking files...");
	}
};
