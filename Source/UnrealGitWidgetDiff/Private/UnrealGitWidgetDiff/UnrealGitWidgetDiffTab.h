// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FUnrealGitWidgetDiffSession;

class FUnrealGitWidgetDiffTab
{
public:
	static void OpenSession(TSharedPtr<FUnrealGitWidgetDiffSession> Session);
	static void CloseActive();

	static TSharedPtr<FUnrealGitWidgetDiffSession> GetPendingWidgetDiffSession();
};