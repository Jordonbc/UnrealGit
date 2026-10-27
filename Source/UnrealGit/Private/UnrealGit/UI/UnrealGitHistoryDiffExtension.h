// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SourceControlWindows.h"

class UToolMenu;

class FUnrealGitHistoryDiffExtension
{
public:
	static void Register();
	static void Unregister();

private:
	static void PopulateDiffMenu(UToolMenu* InMenu);
	static void OnDiffAgainstWorkspace(TArray<USourceControlHistoryWidgetContext::SelectedItem> Selected);
	static void OnDiffSelected(TArray<USourceControlHistoryWidgetContext::SelectedItem> Selected);
	static void OnDiffAgainstPrevious(TArray<USourceControlHistoryWidgetContext::SelectedItem> Selected);
};
