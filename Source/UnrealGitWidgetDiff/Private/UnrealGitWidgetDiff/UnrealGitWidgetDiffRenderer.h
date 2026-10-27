// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageCore.h"

class UWidgetBlueprint;

/**
 * @class FUnrealGitWidgetDiffRenderer
 * Renders a UWidgetBlueprint's designer preview to an FImage and computes
 * a pixel-level difference between two rendered images.
 */
class FUnrealGitWidgetDiffRenderer
{
public:
	/**
	 * Renders the widget blueprint's designer preview to OutImage and captures each widget's rectangle.
	 * @param Blueprint Widget blueprint to render.
	 * @param Mode Preview size mode.
	 * @param OutImage Rendered image.
	 * @param OutWidgetRects Captured widget rectangles keyed by name.
	 * @param OutError Error text on failure.
	 * @return true on success.
	 */
	static bool Render(UWidgetBlueprint* Blueprint, EThumbnailPreviewSizeMode Mode, FImage& OutImage, TMap<FString, FBox2D>& OutWidgetRects, FText& OutError);

	/**
	 * Pads Src into a canvas of CanvasSize, centered, filling the rest with transparent black.
	 * @param Src Source image.
	 * @param CanvasSize Target canvas size.
	 * @return Padded image.
	 */
	static FImage PadToCanvas(const FImage& Src, const FIntPoint& CanvasSize);

	/**
	 * Builds a transparent image with only the changed pixels highlighted in magenta.
	 * @param A First image.
	 * @param B Second image.
	 * @param OutHighlightImage Output highlight image.
	 * @return true on success.
	 */
	static bool MakeHighlight(const FImage& A, const FImage& B, FImage& OutHighlightImage);

	/**
	 * Computes a highlighted difference image between two same-sized images.
	 * @param A First image.
	 * @param B Second image.
	 * @param OutDiffImage Output difference image.
	 * @param OutDiffPixelCount Number of differing pixels.
	 * @param OutDiffBounds Bounding rect of differing pixels.
	 * @param OutDiffRatio Ratio of differing pixels to total area.
	 * @return true on success.
	 */
	static bool ComputeDiff(
		const FImage& A,
		const FImage& B,
		FImage& OutDiffImage,
		int32& OutDiffPixelCount,
		FIntRect& OutDiffBounds,
		float& OutDiffRatio);
};
