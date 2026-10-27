// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffSession.h"

class SUnrealGitWidgetDiffView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUnrealGitWidgetDiffView) {}
		SLATE_ARGUMENT(TSharedPtr<FUnrealGitWidgetDiffSession>, Session)
	SLATE_END_ARGS()

	SUnrealGitWidgetDiffView();
	virtual ~SUnrealGitWidgetDiffView() override;

	void Construct(const FArguments& InArgs);

private:
	enum class EViewMode : uint8 { SideBySide, Difference, Onion };

	TSharedPtr<FUnrealGitWidgetDiffSession> Session;
	EViewMode ViewMode = EViewMode::SideBySide;

	TSharedPtr<SWidgetSwitcher> Switcher;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ModeComboBox;
	TArray<TSharedPtr<FString>> ModeOptions;

	TSharedPtr<SComboBox<TSharedPtr<FString>>> PreviewModeComboBox;
	TArray<TSharedPtr<FString>> PreviewModeOptions;
	TSharedPtr<FString> CurrentPreviewModeOption;
	TSharedPtr<FString> CurrentModeOption;

	FSlateBrush LeftBrush;
	FSlateBrush RightBrush;
	FSlateBrush DiffBrush;

	bool bFitToWindow = true;
	bool bSwapped = false;
	float OnionBlendPercent = 50.0f;

	FDelegateHandle OnUpdatedHandle;

	const FSlateBrush* GetLeftBrush() const { return &LeftBrush; }
	const FSlateBrush* GetRightBrush() const { return &RightBrush; }
	const FSlateBrush* GetDiffBrush() const { return &DiffBrush; }

	bool IsOnionMode() const { return ViewMode == EViewMode::Onion; }

	void OnSessionUpdated();
	void RequestRefresh();
	void UpdateBrushes();
	void UpdateBrush(FSlateBrush& Brush, UTexture2D* Texture);

	int32 GetSwitcherIndex() const;

	void OnModeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	void OnFitChanged(ECheckBoxState NewState);
	FReply OnSwapClicked();
	FReply OnCloseClicked();
	void OnOnionBlendChanged(float NewValue);

	FText GetLeftLabel() const;
	FText GetRightLabel() const;
	FText GetSummaryText() const;
};
