// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @class FUnrealGitWidgetDiffHistoryExtension
 * Adds widget-diff entries to history/revision views.
 */
class FUnrealGitWidgetDiffHistoryExtension
{
public:
	/** Registers the history extension. */
	static void Register();
	/** Unregisters the history extension. */
	static void Unregister();
};
