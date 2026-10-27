// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FUnrealGitWidgetDiffSession;

/**
 * @class FUnrealGitWidgetDiffTab
 * Manages the widget diff tab lifecycle and pending session handoff.
 */
class FUnrealGitWidgetDiffTab
{
public:
	/**
	 * Opens a new tab hosting the given session.
	 * @param Session Session to display.
	 */
	static void OpenSession(TSharedPtr<FUnrealGitWidgetDiffSession> Session);
	/** Closes the currently active widget diff tab. */
	static void CloseActive();

	/**
	 * Returns the pending session waiting to be consumed by the tab.
	 * @return Pending session, or null.
	 */
	static TSharedPtr<FUnrealGitWidgetDiffSession> GetPendingWidgetDiffSession();
};