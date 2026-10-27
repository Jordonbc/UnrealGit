// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WidgetBlueprintEditorUtils.h"

class FUnrealGitWidgetDiffLauncher
{
public:
	static void Launch(const FString& AssetFileName, const FString& LeftRevision, const FString& RightRevision, EThumbnailPreviewSizeMode Mode = EThumbnailPreviewSizeMode::MatchDesignerMode);
};