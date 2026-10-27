// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffToolbarExtension.h"

#include "ContentBrowserMenuContexts.h"
#include "Framework/Commands/UICommandList.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "UMGEditorModule.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintEditor.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffLauncher.h"

#define LOCTEXT_NAMESPACE "UnrealGitWidgetDiffToolbar"

void FUnrealGitWidgetDiffToolbarExtension::Register()
{
	IUMGEditorModule& M = FModuleManager::LoadModuleChecked<IUMGEditorModule>("UMGEditor");
	auto Extender = [](const TSharedRef<FUICommandList> Cmd, TSharedRef<FWidgetBlueprintEditor> Ed) -> TSharedRef<FExtender>
	{
		TSharedRef<FExtender> E = MakeShareable(new FExtender);
		// Add a simple toolbar button by extending the "Asset" toolbar section.
		E->AddToolBarExtension("Asset", EExtensionHook::After, Cmd,
			FToolBarExtensionDelegate::CreateLambda([Ed](FToolBarBuilder& B)
			{
				B.AddToolBarButton(
					FUIAction(FExecuteAction::CreateLambda([Ed]()
					{
						if (UBlueprint* BP = Ed->GetBlueprintObj())
						{
							const FString Filename = SourceControlHelpers::PackageFilename(BP->GetPathName());
							if (!Filename.IsEmpty())
							{
								FUnrealGitWidgetDiffLauncher::Launch(Filename, FString(), FString());
							}
						}
					})),
					NAME_None,
					LOCTEXT("VisualDiff", "Visual Diff"),
					LOCTEXT("VisualDiffTooltip", "Visually diff this widget against HEAD."),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Diff")
				);
			}));
		return E;
	};
	M.AddWidgetEditorToolbarExtender(IUMGEditorModule::FWidgetEditorToolbarExtender::CreateLambda(Extender));
}

void FUnrealGitWidgetDiffToolbarExtension::Unregister()
{
	// Extenders are stored by the module; nothing to remove individually.
}

#undef LOCTEXT_NAMESPACE
