// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @class FUnrealGitWidgetDiffToolbarExtension
 * Adds widget-diff entries to toolbar menus.
 */
class FUnrealGitWidgetDiffToolbarExtension
{
public:
	/** Registers the toolbar extension. */
	static void Register();
	/** Unregisters the toolbar extension. */
	static void Unregister();
};
