// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffModule.h"

#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/SUnrealGitWidgetDiffView.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffAssetContextMenuExtension.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffHistoryExtension.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffTab.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffToolbarExtension.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffSession.h"

#define LOCTEXT_NAMESPACE "UnrealGitWidgetDiffModule"

static TSharedRef<SDockTab> SpawnUnrealGitWidgetDiffTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedPtr<FUnrealGitWidgetDiffSession> Session = FUnrealGitWidgetDiffTab::GetPendingWidgetDiffSession();

	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);

	DockTab->SetContent(
		SNew(SUnrealGitWidgetDiffView).Session(Session).OwnerTab(DockTab)
	);

	return DockTab;
}

void IUnrealGitWidgetDiffModule::StartupModule()
{
	FUnrealGitWidgetDiffToolbarExtension::Register();
	FUnrealGitWidgetDiffAssetContextMenuExtension::Register();
	FUnrealGitWidgetDiffHistoryExtension::Register();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		TEXT("UnrealGitWidgetDiff"),
		FOnSpawnTab::CreateStatic(&SpawnUnrealGitWidgetDiffTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Widget Diff"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void IUnrealGitWidgetDiffModule::ShutdownModule()
{
	FUnrealGitWidgetDiffHistoryExtension::Unregister();
	FUnrealGitWidgetDiffAssetContextMenuExtension::Unregister();
	FUnrealGitWidgetDiffToolbarExtension::Unregister();
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("UnrealGitWidgetDiff"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(IUnrealGitWidgetDiffModule, UnrealGitWidgetDiff)