// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class UWidgetBlueprint;

/**
 * Builds a live preview widget instance from a widget blueprint, optionally
 * hosting its Slate widget in a virtual window to compute the design size.
 * @return true when the widget (and, if requested, its Slate preview) was created.
 */
bool CreatePreviewWidget(
	UWidgetBlueprint* Blueprint,
	EThumbnailPreviewSizeMode Mode,
	TStrongObjectPtr<class UUserWidget>& OutWidget,
	TSharedPtr<class SWidget>& OutSlate,
	FVector2D& OutDesignSize,
	bool bBuildSlatePreview = true,
	const FVector2D& PreviewWindowSize = FVector2D(1024.0f, 1024.0f));