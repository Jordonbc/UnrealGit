// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffStyle.h"

FLinearColor GetChangeColor(EUnrealGitWidgetChange Change)
{
	switch (Change)
	{
	case EUnrealGitWidgetChange::Added: return FLinearColor(0.3f, 0.9f, 0.3f, 1.0f);
	case EUnrealGitWidgetChange::Removed: return FLinearColor(1.0f, 0.3f, 0.3f, 1.0f);
	case EUnrealGitWidgetChange::Modified: return FLinearColor(1.0f, 0.8f, 0.2f, 1.0f);
	default: return FLinearColor(0.75f, 0.75f, 0.75f, 1.0f);
	}
}

FLinearColor GetChangeBadgeColor(EUnrealGitWidgetChange Change)
{
	switch (Change)
	{
	case EUnrealGitWidgetChange::Added: return FLinearColor(0.2f, 0.8f, 0.2f, 1.0f);
	case EUnrealGitWidgetChange::Removed: return FLinearColor(0.9f, 0.2f, 0.2f, 1.0f);
	case EUnrealGitWidgetChange::Modified: return FLinearColor(0.95f, 0.75f, 0.1f, 1.0f);
	default: return FLinearColor(0.6f, 0.6f, 0.6f, 1.0f);
	}
}

FLinearColor GetChangeOutlineColor(EUnrealGitWidgetChange Change)
{
	switch (Change)
	{
	case EUnrealGitWidgetChange::Added: return FLinearColor(0.15f, 1.0f, 0.25f, 1.0f);
	case EUnrealGitWidgetChange::Removed: return FLinearColor(1.0f, 0.2f, 0.2f, 1.0f);
	case EUnrealGitWidgetChange::Modified: return FLinearColor(1.0f, 0.65f, 0.05f, 1.0f);
	default: return FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

FString GetChangeGlyph(EUnrealGitWidgetChange Change)
{
	switch (Change)
	{
	case EUnrealGitWidgetChange::Added: return TEXT("+");
	case EUnrealGitWidgetChange::Removed: return TEXT("-");
	case EUnrealGitWidgetChange::Modified: return TEXT("~");
	default: return TEXT("~");
	}
}