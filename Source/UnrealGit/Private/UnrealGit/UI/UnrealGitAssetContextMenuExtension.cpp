// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/UI/UnrealGitAssetContextMenuExtension.h"

#include "ContentBrowserMenuContexts.h"
#include "Framework/Notifications/NotificationManager.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlHelpers.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "UnrealGit/Private/UnrealGit/SourceControl/UnrealGitLockOperations.h"
#include "UnrealGit/Settings/UnrealGitProjectSettings.h"
#include "UnrealGit/SourceControl/UnrealGitSourceControlProvider.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "UnrealGitAssetContextMenuExtension"

static const FName RevisionControlSubMenuName(TEXT("ContentBrowser.AssetContextMenu.SourceControlSubMenu"));

static bool IsUnrealGitProviderActive()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	if (!SourceControlModule.IsEnabled())
	{
		return false;
	}

	return SourceControlModule.GetProvider().GetName() == FName(TEXT("UnrealGit"));
}

static bool AreLfsLocksEnabled()
{
	const UUnrealGitProjectSettings* ProjectSettings = GetDefault<UUnrealGitProjectSettings>();
	return ProjectSettings && ProjectSettings->bEnableLfsLocks;
}

static TArray<FString> GetSelectedPackageFilenames(const UContentBrowserAssetContextMenuContext* Context)
{
	TArray<FString> Filenames;

	if (!Context)
	{
		return Filenames;
	}

	for (const FAssetData& Asset : Context->SelectedAssets)
	{
		const FString Filename = SourceControlHelpers::PackageFilename(Asset.PackageName.ToString());
		if (!Filename.IsEmpty())
		{
			Filenames.Add(Filename);
		}
	}

	return Filenames;
}

static void OnForceUnlock(const TArray<FString>& Filenames)
{
	if (Filenames.Num() == 0)
	{
		return;
	}

	const FSourceControlOperationComplete OnComplete = FSourceControlOperationComplete::CreateLambda(
		[](const FSourceControlOperationRef& /*InOperation*/, ECommandResult::Type InResult)
		{
			FNotificationInfo Info(InResult == ECommandResult::Succeeded
				? LOCTEXT("ForceUnlockSucceeded", "Force unlock completed.")
				: LOCTEXT("ForceUnlockFailed", "Force unlock failed. See the Source Control log for details."));
			Info.bUseSuccessFailIcons = true;
			Info.ExpireDuration = 5.0f;
			Info.Image = FAppStyle::GetBrush(InResult == ECommandResult::Succeeded
				? TEXT("NotificationList.SuccessImage")
				: TEXT("NotificationList.FailImage"));

			FSlateNotificationManager::Get().AddNotification(Info);
		});

	ISourceControlModule::Get().GetProvider().Execute(
		ISourceControlOperation::Create<FUnrealGitForceUnlockOperation>(),
		nullptr,
		Filenames,
		EConcurrency::Asynchronous,
		OnComplete);
}

static void PopulateRevisionControlSubMenu(UToolMenu* InMenu)
{
	if (!InMenu || !IsUnrealGitProviderActive())
	{
		return;
	}

	const UContentBrowserAssetContextMenuContext* Context = InMenu->FindContext<UContentBrowserAssetContextMenuContext>();
	const TArray<FString> Filenames = GetSelectedPackageFilenames(Context);
	if (Filenames.Num() == 0)
	{
		return;
	}

	const bool bLocksEnabled = AreLfsLocksEnabled();

	FToolMenuSection& Section = InMenu->FindOrAddSection(TEXT("UnrealGitForceUnlock"), FText::GetEmpty());

	Section.AddMenuEntry(
		TEXT("UnrealGitForceUnlock"),
		LOCTEXT("UnrealGitForceUnlock", "Force Unlock"),
		LOCTEXT("UnrealGitForceUnlockTooltip", "Release a Git LFS lock owned by any user. Requires permission on the remote."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Revert"),
		FUIAction(
			FExecuteAction::CreateLambda([Filenames]() { OnForceUnlock(Filenames); }),
			FCanExecuteAction::CreateLambda([bLocksEnabled]() { return bLocksEnabled; })
		)
	);
}

void FUnrealGitAssetContextMenuExtension::Register()
{
	UToolMenus* ToolMenus = UToolMenus::TryGet();
	if (!ToolMenus)
	{
		return;
	}

	// Force Unlock lives inside the asset context menu's "Revision Control" submenu.
	if (UToolMenu* RevisionControlMenu = ToolMenus->ExtendMenu(RevisionControlSubMenuName))
	{
		RevisionControlMenu->AddDynamicSection(TEXT("UnrealGitForceUnlockDynamic"), FNewToolMenuDelegate::CreateStatic(&PopulateRevisionControlSubMenu));
	}
}

void FUnrealGitAssetContextMenuExtension::Unregister()
{
	UToolMenus* ToolMenus = UToolMenus::TryGet();
	if (!ToolMenus)
	{
		return;
	}

	if (UToolMenu* RevisionControlMenu = ToolMenus->ExtendMenu(RevisionControlSubMenuName))
	{
		RevisionControlMenu->RemoveSection(TEXT("UnrealGitForceUnlockDynamic"));
		RevisionControlMenu->RemoveSection(TEXT("UnrealGitForceUnlock"));
	}
}

#undef LOCTEXT_NAMESPACE
