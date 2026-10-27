// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Adds a Force Unlock entry to the Content Browser's "Revision Control" submenu.
 *
 * Responsibility:
 * - Resolves the selected assets to package filenames and executes the force-unlock
 *   operation through the active source control provider.
 */
class FUnrealGitAssetContextMenuExtension final
{
public:
	static void Register();
	static void Unregister();
};
