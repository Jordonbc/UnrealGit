// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffHistoryExtension.h"

#include "AssetToolsModule.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlRevision.h"
#include "ISourceControlState.h"
#include "ToolMenus.h"
#include "ISourceControlRevision.h"
#include "SourceControlWindows.h"
#include "ISourceControlState.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffLauncher.h"

#define LOCTEXT_NAMESPACE "UnrealGitWidgetDiffHistory"

static bool IsUnrealGitProviderActiveHistory()
{
	ISourceControlModule& M = ISourceControlModule::Get();
	if (!M.IsEnabled()) return false;
	return M.GetProvider().GetName() == FName(TEXT("UnrealGit"));
}

static void PopulateHistoryMenu(UToolMenu* InMenu)
{
	if (!InMenu || !IsUnrealGitProviderActiveHistory()) return;
	USourceControlHistoryWidgetContext* Context = InMenu->FindContext<USourceControlHistoryWidgetContext>();
	if (!Context) return;
	const TArray<USourceControlHistoryWidgetContext::SelectedItem> Selected = Context->GetSelectedItems();
	if (Selected.Num() == 0) return;

	FToolMenuSection& Section = InMenu->AddSection(TEXT("UnrealGitWidgetDiff"), FText::GetEmpty());

	if (Selected.Num() == 1)
	{
		Section.AddMenuEntry(
			TEXT("VisualDiffAgainstWorkspace"),
			LOCTEXT("VisualDiffAgainstWorkspace", "Visual Diff Against Workspace"),
			LOCTEXT("VisualDiffAgainstWorkspaceTooltip", "Render and visually compare this revision against the working copy."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Diff"),
			FUIAction(FExecuteAction::CreateLambda([Selected]()
			{
				FUnrealGitWidgetDiffLauncher::Launch(Selected[0].FileName, Selected[0].Revision, FString());
			}))
		);

		Section.AddMenuEntry(
			TEXT("VisualDiffAgainstPrevious"),
			LOCTEXT("VisualDiffAgainstPrevious", "Visual Diff Against Previous"),
			LOCTEXT("VisualDiffAgainstPreviousTooltip", "Render and visually compare this revision against the previous one."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Diff"),
			FUIAction(
				FExecuteAction::CreateLambda([Selected]()
				{
					ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
					FSourceControlStatePtr State = Provider.GetState(Selected[0].FileName, EStateCacheUsage::Use);
					if (!State.IsValid()) return;
					const TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Cur = State->FindHistoryRevision(Selected[0].Revision);
					if (!Cur.IsValid()) return;
					int32 Idx = INDEX_NONE;
					for (int32 i = 0; i < State->GetHistorySize(); ++i)
					{
						if (State->GetHistoryItem(i) == Cur) { Idx = i; break; }
					}
					if (Idx == INDEX_NONE || Idx + 1 >= State->GetHistorySize()) return;
					const TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Prev = State->GetHistoryItem(Idx + 1);
					if (!Prev.IsValid()) return;
					FUnrealGitWidgetDiffLauncher::Launch(Selected[0].FileName, Prev->GetRevision(), Selected[0].Revision);
				}),
				FCanExecuteAction::CreateLambda([Selected]()
				{
					ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
					FSourceControlStatePtr State = Provider.GetState(Selected[0].FileName, EStateCacheUsage::Use);
					if (!State.IsValid()) return false;
					const TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Cur = State->FindHistoryRevision(Selected[0].Revision);
					if (!Cur.IsValid()) return false;
					int32 Idx = INDEX_NONE;
					for (int32 i = 0; i < State->GetHistorySize(); ++i)
					{
						if (State->GetHistoryItem(i) == Cur) { Idx = i; break; }
					}
					return Idx != INDEX_NONE && Idx + 1 < State->GetHistorySize();
				})
			)
		);
	}

	if (Selected.Num() == 2)
	{
		Section.AddMenuEntry(
			TEXT("VisualDiffSelected"),
			LOCTEXT("VisualDiffSelected", "Visual Diff Selected"),
			LOCTEXT("VisualDiffSelectedTooltip", "Render and visually compare the two selected revisions."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Diff"),
			FUIAction(FExecuteAction::CreateLambda([Selected]()
			{
				ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
				FSourceControlStatePtr State = Provider.GetState(Selected[0].FileName, EStateCacheUsage::Use);
				if (!State.IsValid()) return;
				const TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> RevA = State->FindHistoryRevision(Selected[0].Revision);
				const TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> RevB = State->FindHistoryRevision(Selected[1].Revision);
				if (!RevA.IsValid() || !RevB.IsValid()) return;
				int32 IdxA = INDEX_NONE, IdxB = INDEX_NONE;
				for (int32 i = 0; i < State->GetHistorySize(); ++i)
				{
					if (State->GetHistoryItem(i) == RevA) IdxA = i;
					if (State->GetHistoryItem(i) == RevB) IdxB = i;
				}
				const bool bAOlder = (IdxA > IdxB);
				const FString OldRev = bAOlder ? Selected[0].Revision : Selected[1].Revision;
				const FString NewRev = bAOlder ? Selected[1].Revision : Selected[0].Revision;
				FUnrealGitWidgetDiffLauncher::Launch(Selected[0].FileName, OldRev, NewRev);
			}))
		);
	}
}

void FUnrealGitWidgetDiffHistoryExtension::Register()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus) return;
	UToolMenu* Menu = ToolMenus->FindMenu(TEXT("RevisionControl.History.ContextMenu"));
	if (!Menu)
	{
		Menu = ToolMenus->RegisterMenu(TEXT("RevisionControl.History.ContextMenu"));
	}
	if (Menu)
	{
		Menu->AddDynamicSection(TEXT("UnrealGitWidgetDiffHistoryDynamic"), FNewToolMenuDelegate::CreateStatic(&PopulateHistoryMenu));
	}
}

void FUnrealGitWidgetDiffHistoryExtension::Unregister()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus) return;
	if (UToolMenu* Menu = ToolMenus->FindMenu(TEXT("RevisionControl.History.ContextMenu")))
	{
		Menu->RemoveSection(TEXT("UnrealGitWidgetDiffHistoryDynamic"));
	}
}

#undef LOCTEXT_NAMESPACE
