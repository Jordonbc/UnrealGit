// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageCore.h"

class UWidgetBlueprint;

/**
 * Renders a UWidgetBlueprint's designer preview to an FImage and computes
 * a pixel-level difference between two rendered images.
 */
class FUnrealGitWidgetDiffRenderer
{
public:
	/** Renders the widget blueprint's designer preview to OutImage. */
	static bool Render(UWidgetBlueprint* Blueprint, EThumbnailPreviewSizeMode Mode, FImage& OutImage, FText& OutError);

	/** Pads Src into a canvas of CanvasSize, centered, filling the rest with transparent black. */
	static FImage PadToCanvas(const FImage& Src, const FIntPoint& CanvasSize);

	/** Computes a highlighted difference image between two same-sized images. */
	static bool ComputeDiff(
		const FImage& A,
		const FImage& B,
		FImage& OutDiffImage,
		int32& OutDiffPixelCount,
		FIntRect& OutDiffBounds,
		float& OutDiffRatio);
};
