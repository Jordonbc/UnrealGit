// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @class FUnrealGitAssetContextMenuExtension
 * Adds a Force Unlock entry to the Content Browser's "Revision Control" submenu.
 *
 * Responsibility:
 * - Resolves the selected assets to package filenames and executes the force-unlock
 *   operation through the active source control provider.
 */
class FUnrealGitAssetContextMenuExtension final
{
public:
	/** Registers the context menu extension. */
	static void Register();

	/** Unregisters the context menu extension. */
	static void Unregister();
};
