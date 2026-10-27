// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.
#include "UnrealGit/UI/UnrealGitHistoryDiffExtension.h"

#include "AssetDefinition.h"
#include "AssetToolsModule.h"
#include "DiffUtils.h"
#include "IAssetTools.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlRevision.h"
#include "ISourceControlState.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "UnrealGitHistoryDiff"

namespace UnrealGitHistoryDiff
{
	static TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> FindRevision(const FString& FileName, const FString& RevisionString)
	{
		ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
		FSourceControlStatePtr State = Provider.GetState(FileName, EStateCacheUsage::Use);
		if (!State.IsValid())
		{
			return TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>();
		}
		return State->FindHistoryRevision(RevisionString);
	}

	static bool LoadRevisionAsset(const TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>& Revision, UObject*& OutAsset)
	{
		OutAsset = nullptr;
		if (!Revision.IsValid())
		{
			return false;
		}

		UPackage* Package = DiffUtils::LoadPackageForDiff(Revision);
		if (!Package)
		{
			return false;
		}

		const FString AssetName = FPaths::GetBaseFilename(Revision->GetFilename());
		OutAsset = FindObject<UObject>(Package, *AssetName);
		if (!OutAsset)
		{
			OutAsset = Package->FindAssetInPackage();
		}
		return OutAsset != nullptr;
	}

	static bool LoadWorkspaceAsset(const FString& FileName, UObject*& OutAsset)
	{
		OutAsset = nullptr;

		FString PackageName;
		if (!FPackageName::TryConvertFilenameToLongPackageName(FileName, PackageName))
		{
			return false;
		}

		UPackage* Package = FindObject<UPackage>(nullptr, *PackageName);
		if (!Package)
		{
			Package = LoadPackage(nullptr, *PackageName, LOAD_None);
		}
		if (!Package)
		{
			return false;
		}

		const FString AssetName = FPaths::GetBaseFilename(FileName);
		OutAsset = FindObject<UObject>(Package, *AssetName);
		if (!OutAsset)
		{
			OutAsset = Package->FindAssetInPackage();
		}
		return OutAsset != nullptr;
	}

	static FRevisionInfo MakeRevisionInfo(const TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>& Revision)
	{
		FRevisionInfo Info;
		Info.Revision = Revision.IsValid() ? Revision->GetRevision() : FString();
		Info.Date = Revision.IsValid() ? Revision->GetDate() : FDateTime();
		return Info;
	}

	static void ShowLoadError()
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("UnableToLoad", "Unable to load assets to diff. Content may no longer be supported?"));
	}
}

void FUnrealGitHistoryDiffExtension::Register()
{
	UToolMenus* ToolMenus = UToolMenus::TryGet();
	if (!ToolMenus)
	{
		return;
	}

	// ExtendMenu keeps the engine's own registration intact; RegisterMenu would
	// mark the shared menu as plugin-owned and stop the engine adding its section.
	if (UToolMenu* Menu = ToolMenus->ExtendMenu(TEXT("RevisionControl.History.ContextMenu")))
	{
		Menu->AddDynamicSection(TEXT("UnrealGitHistoryDiffDynamic"), FNewToolMenuDelegate::CreateStatic(&FUnrealGitHistoryDiffExtension::PopulateDiffMenu));
	}
}

void FUnrealGitHistoryDiffExtension::Unregister()
{
	// The ToolMenus singleton is destroyed before modules unload at editor exit,
	// so it can legitimately be unavailable here. Remove only our own section.
	UToolMenus* ToolMenus = UToolMenus::TryGet();
	if (!ToolMenus)
	{
		return;
	}

	if (UToolMenu* Menu = ToolMenus->FindMenu(TEXT("RevisionControl.History.ContextMenu")))
	{
		Menu->RemoveSection(TEXT("UnrealGitHistoryDiffDynamic"));
	}
}

void FUnrealGitHistoryDiffExtension::PopulateDiffMenu(UToolMenu* InMenu)
{
	USourceControlHistoryWidgetContext* Context = InMenu->FindContext<USourceControlHistoryWidgetContext>();
	if (!Context)
	{
		return;
	}

	const TArray<USourceControlHistoryWidgetContext::SelectedItem> Selected = Context->GetSelectedItems();

	FToolMenuSection& Section = InMenu->AddSection(TEXT("DiffTools"));
	Section.AddMenuEntry(
		TEXT("DiffAgainstPrevious"),
		LOCTEXT("DiffAgainstPrevious", "Diff Against Previous Revision"),
		LOCTEXT("DiffAgainstPreviousTooltip", "See changes between this revision and the previous one."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Diff"),
		FUIAction(
			FExecuteAction::CreateStatic(&FUnrealGitHistoryDiffExtension::OnDiffAgainstPrevious, Selected),
			FCanExecuteAction::CreateLambda([Selected]() { return Selected.Num() == 1; })
		)
	);

	Section.AddMenuEntry(
		TEXT("DiffAgainstWorkspace"),
		LOCTEXT("DiffAgainstWorkspace", "Diff Against Workspace File"),
		LOCTEXT("DiffAgainstWorkspaceTooltip", "See changes between this revision and your version of the asset."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Diff"),
		FUIAction(
			FExecuteAction::CreateStatic(&FUnrealGitHistoryDiffExtension::OnDiffAgainstWorkspace, Selected),
			FCanExecuteAction::CreateLambda([Selected]() { return Selected.Num() == 1; })
		)
	);

	Section.AddMenuEntry(
		TEXT("DiffSelected"),
		LOCTEXT("DiffSelected", "Diff Selected"),
		LOCTEXT("DiffSelectedTooltip", "See changes between the two selected revisions."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Diff"),
		FUIAction(
			FExecuteAction::CreateStatic(&FUnrealGitHistoryDiffExtension::OnDiffSelected, Selected),
			FCanExecuteAction::CreateLambda([Selected]() { return Selected.Num() == 2; })
		)
	);
}

void FUnrealGitHistoryDiffExtension::OnDiffAgainstWorkspace(TArray<USourceControlHistoryWidgetContext::SelectedItem> Selected)
{
	if (Selected.Num() != 1)
	{
		return;
	}

	const TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision = UnrealGitHistoryDiff::FindRevision(Selected[0].FileName, Selected[0].Revision);
	UObject* HistoricalAsset = nullptr;
	UObject* WorkspaceAsset = nullptr;
	if (!UnrealGitHistoryDiff::LoadRevisionAsset(Revision, HistoricalAsset) || !UnrealGitHistoryDiff::LoadWorkspaceAsset(Selected[0].FileName, WorkspaceAsset))
	{
		UnrealGitHistoryDiff::ShowLoadError();
		return;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	FRevisionInfo NewRevisionInfo;
	NewRevisionInfo.Revision = TEXT("");
	AssetToolsModule.Get().DiffAssets(HistoricalAsset, WorkspaceAsset, UnrealGitHistoryDiff::MakeRevisionInfo(Revision), NewRevisionInfo);
}

void FUnrealGitHistoryDiffExtension::OnDiffSelected(TArray<USourceControlHistoryWidgetContext::SelectedItem> Selected)
{
	if (Selected.Num() != 2)
	{
		return;
	}

	const TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> RevisionA = UnrealGitHistoryDiff::FindRevision(Selected[0].FileName, Selected[0].Revision);
	const TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> RevisionB = UnrealGitHistoryDiff::FindRevision(Selected[1].FileName, Selected[1].Revision);
	UObject* AssetA = nullptr;
	UObject* AssetB = nullptr;
	if (!UnrealGitHistoryDiff::LoadRevisionAsset(RevisionA, AssetA) || !UnrealGitHistoryDiff::LoadRevisionAsset(RevisionB, AssetB))
	{
		UnrealGitHistoryDiff::ShowLoadError();
		return;
	}

	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr State = Provider.GetState(Selected[0].FileName, EStateCacheUsage::Use);
	int32 IndexA = INDEX_NONE;
	int32 IndexB = INDEX_NONE;
	if (State.IsValid())
	{
		for (int32 Index = 0; Index < State->GetHistorySize(); ++Index)
		{
			const TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Item = State->GetHistoryItem(Index);
			if (Item == RevisionA)
			{
				IndexA = Index;
			}
			if (Item == RevisionB)
			{
				IndexB = Index;
			}
		}
	}

	const bool bAIsOlder = (IndexA > IndexB);
	const TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> OldRevision = bAIsOlder ? RevisionA : RevisionB;
	const TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> NewRevision = bAIsOlder ? RevisionB : RevisionA;
	UObject* OldAsset = bAIsOlder ? AssetA : AssetB;
	UObject* NewAsset = bAIsOlder ? AssetB : AssetA;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	AssetToolsModule.Get().DiffAssets(OldAsset, NewAsset, UnrealGitHistoryDiff::MakeRevisionInfo(OldRevision), UnrealGitHistoryDiff::MakeRevisionInfo(NewRevision));
}

void FUnrealGitHistoryDiffExtension::OnDiffAgainstPrevious(TArray<USourceControlHistoryWidgetContext::SelectedItem> Selected)
{
	if (Selected.Num() != 1)
	{
		return;
	}

	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr State = Provider.GetState(Selected[0].FileName, EStateCacheUsage::Use);
	if (!State.IsValid())
	{
		return;
	}

	const TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> CurrentRevision = State->FindHistoryRevision(Selected[0].Revision);
	if (!CurrentRevision.IsValid())
	{
		return;
	}

	int32 CurrentIndex = INDEX_NONE;
	for (int32 Index = 0; Index < State->GetHistorySize(); ++Index)
	{
		if (State->GetHistoryItem(Index) == CurrentRevision)
		{
			CurrentIndex = Index;
			break;
		}
	}
	if (CurrentIndex == INDEX_NONE || CurrentIndex + 1 >= State->GetHistorySize())
	{
		return;
	}

	const TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> PreviousRevision = State->GetHistoryItem(CurrentIndex + 1);
	UObject* CurrentAsset = nullptr;
	UObject* PreviousAsset = nullptr;
	if (!UnrealGitHistoryDiff::LoadRevisionAsset(CurrentRevision, CurrentAsset) || !UnrealGitHistoryDiff::LoadRevisionAsset(PreviousRevision, PreviousAsset))
	{
		UnrealGitHistoryDiff::ShowLoadError();
		return;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	AssetToolsModule.Get().DiffAssets(PreviousAsset, CurrentAsset, UnrealGitHistoryDiff::MakeRevisionInfo(PreviousRevision), UnrealGitHistoryDiff::MakeRevisionInfo(CurrentRevision));
}

#undef LOCTEXT_NAMESPACE
