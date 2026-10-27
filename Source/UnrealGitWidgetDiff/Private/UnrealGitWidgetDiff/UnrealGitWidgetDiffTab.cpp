// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffTab.h"

#include "Framework/Docking/TabManager.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffSession.h"
#include "Widgets/Docking/SDockTab.h"

static TSharedPtr<FUnrealGitWidgetDiffSession> GPendingWidgetDiffSession;

void FUnrealGitWidgetDiffTab::OpenSession(TSharedPtr<FUnrealGitWidgetDiffSession> Session)
{
	GPendingWidgetDiffSession = Session;

	if (TSharedPtr<FTabManager> TabManager = FGlobalTabmanager::Get())
	{
		TabManager->TryInvokeTab(FTabId(TEXT("UnrealGitWidgetDiff")));
	}
}

void FUnrealGitWidgetDiffTab::CloseActive()
{
	if (TSharedPtr<FTabManager> TabManager = FGlobalTabmanager::Get())
	{
		TSharedPtr<SDockTab> ExistingTab = TabManager->FindExistingLiveTab(FTabId(TEXT("UnrealGitWidgetDiff")));
		if (ExistingTab.IsValid())
		{
			ExistingTab->RequestCloseTab();
		}
	}
	GPendingWidgetDiffSession.Reset();
}

TSharedPtr<FUnrealGitWidgetDiffSession> FUnrealGitWidgetDiffTab::GetPendingWidgetDiffSession()
{
	return GPendingWidgetDiffSession;
}