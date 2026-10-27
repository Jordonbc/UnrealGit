// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @class FUnrealGitWidgetDiffAssetContextMenuExtension
 * Adds widget-diff entries to asset context menus.
 */
class FUnrealGitWidgetDiffAssetContextMenuExtension
{
public:
	/** Registers the context menu extension. */
	static void Register();
	/** Unregisters the context menu extension. */
	static void Unregister();
};
