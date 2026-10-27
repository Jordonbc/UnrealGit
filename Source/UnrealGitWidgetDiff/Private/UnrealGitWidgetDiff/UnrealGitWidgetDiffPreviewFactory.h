// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class UWidgetBlueprint;

/**
 * Builds a live preview widget instance from a widget blueprint, optionally
 * hosting its Slate widget in a virtual window to compute the design size.
 * @param Blueprint Widget blueprint to preview.
 * @param Mode Preview size mode.
 * @param OutWidget Created user widget.
 * @param OutSlate Created Slate widget when bBuildSlatePreview is true.
 * @param OutDesignSize Computed design size.
 * @param bBuildSlatePreview When true, instantiates and hosts the Slate widget in a
 *        virtual window to measure the design size; when false, only the user
 *        widget is created and OutSlate is left null.
 * @param PreviewWindowSize Size of the virtual window used for measurement.
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