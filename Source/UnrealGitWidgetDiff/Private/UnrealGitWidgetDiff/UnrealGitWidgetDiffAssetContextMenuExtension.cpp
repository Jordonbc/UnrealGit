// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffAssetContextMenuExtension.h"

#include "ContentBrowserMenuContexts.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffLauncher.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "UnrealGitWidgetDiffAssetContextMenu"

static const FName RevisionControlSubMenuName(TEXT("ContentBrowser.AssetContextMenu.SourceControlSubMenu"));

static bool IsUnrealGitProviderActive()
{
	ISourceControlModule& M = ISourceControlModule::Get();
	if (!M.IsEnabled()) return false;
	return M.GetProvider().GetName() == FName(TEXT("UnrealGit"));
}

static bool AreSelectedWidgetBlueprints(const UContentBrowserAssetContextMenuContext* Context)
{
	if (!Context || Context->SelectedAssets.Num() != 1) return false;
	return Context->SelectedAssets[0].GetClass() && Context->SelectedAssets[0].GetClass()->IsChildOf(UWidgetBlueprint::StaticClass());
}

static void PopulateMenu(UToolMenu* InMenu)
{
	if (!InMenu || !IsUnrealGitProviderActive()) return;
	const UContentBrowserAssetContextMenuContext* Context = InMenu->FindContext<UContentBrowserAssetContextMenuContext>();
	if (!Context || !AreSelectedWidgetBlueprints(Context)) return;

	TArray<FString> Filenames;
	for (const FAssetData& Asset : Context->SelectedAssets)
	{
		const FString Filename = SourceControlHelpers::PackageFilename(Asset.PackageName.ToString());
		if (!Filename.IsEmpty()) Filenames.Add(Filename);
	}
	if (Filenames.Num() == 0) return;

	FToolMenuSection& Section = InMenu->FindOrAddSection(TEXT("UnrealGitWidgetDiff"), FText::GetEmpty());
	Section.AddMenuEntry(
		TEXT("UnrealGitVisualDiffWidget"),
		LOCTEXT("VisualDiffWidget", "Visual Diff Widget"),
		LOCTEXT("VisualDiffWidgetTooltip", "Render and visually compare this widget against HEAD."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Diff"),
		FUIAction(FExecuteAction::CreateLambda([Filenames]()
		{
			for (const FString& F : Filenames)
			{
				FUnrealGitWidgetDiffLauncher::Launch(F, FString(), FString());
			}
		}))
	);
}

void FUnrealGitWidgetDiffAssetContextMenuExtension::Register()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus) return;
	if (UToolMenu* Menu = ToolMenus->ExtendMenu(RevisionControlSubMenuName))
	{
		Menu->AddDynamicSection(TEXT("UnrealGitWidgetDiffDynamic"), FNewToolMenuDelegate::CreateStatic(&PopulateMenu));
	}
}

void FUnrealGitWidgetDiffAssetContextMenuExtension::Unregister()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus) return;
	if (UToolMenu* Menu = ToolMenus->ExtendMenu(RevisionControlSubMenuName))
	{
		Menu->RemoveSection(TEXT("UnrealGitWidgetDiffDynamic"));
	}
}

#undef LOCTEXT_NAMESPACE
