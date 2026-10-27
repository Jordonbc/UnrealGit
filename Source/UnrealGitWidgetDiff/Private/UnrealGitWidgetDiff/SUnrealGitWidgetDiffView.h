// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"
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
	FSlateBrush HighlightBrush;

	TSharedPtr<class SUnrealGitWidgetDiffCanvas> LeftCanvas;
	TSharedPtr<class SUnrealGitWidgetDiffCanvas> RightCanvas;
	TSharedPtr<class SUnrealGitWidgetDiffCanvas> DiffCanvas;
	TSharedPtr<struct FUnrealGitWidgetDiffCanvasState> CanvasState;
	TSharedPtr<TArray<FUnrealGitWidgetNode>> CanvasNodes;

	TStrongObjectPtr<class UUserWidget> LeftLiveWidget;
	TStrongObjectPtr<class UUserWidget> RightLiveWidget;
	TSharedPtr<SWidget> LeftPreviewWidget;
	TSharedPtr<SWidget> RightPreviewWidget;
	FVector2D LeftDesignSize = FVector2D::ZeroVector;
	FVector2D RightDesignSize = FVector2D::ZeroVector;
	bool bLivePreviewsBuilt = false;

	void EnsureLivePreviews();
	void SelectWidgetByName(const FString& Name);

	bool bFitToWindow = true;
	bool bSwapped = false;
	float OnionBlendPercent = 50.0f;

	FDelegateHandle OnUpdatedHandle;

	const FSlateBrush* GetLeftBrush() const { return &LeftBrush; }
	const FSlateBrush* GetRightBrush() const { return &RightBrush; }
	const FSlateBrush* GetDiffBrush() const { return &DiffBrush; }
	const FSlateBrush* GetHighlightBrush() const { return &HighlightBrush; }

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

	TSharedPtr<STreeView<TSharedPtr<FUnrealGitWidgetNode>>> HierarchyList;
	TArray<TSharedPtr<FUnrealGitWidgetNode>> HierarchyItems;
	TArray<TSharedPtr<FUnrealGitWidgetNode>> HierarchyRoots;

	void OnGetHierarchyChildren(TSharedPtr<FUnrealGitWidgetNode> Item, TArray<TSharedPtr<FUnrealGitWidgetNode>>& OutChildren) const;
	struct FDiffEntry
	{
		FString Name;
		FString TypeName;
		FString Path;
		EUnrealGitWidgetChange Change = EUnrealGitWidgetChange::Unchanged;
		TArray<FString> Properties;
	};

	TSharedPtr<SListView<TSharedPtr<FDiffEntry>>> PropertyList;
	TArray<TSharedPtr<FDiffEntry>> PropertyItems;
	TArray<TSharedPtr<FDiffEntry>> AllDiffEntries;
	TSharedPtr<STextBlock> DiffSummaryText;

	bool bShowAdded = true;
	bool bShowRemoved = true;
	bool bShowModified = true;

	void RebuildDiffEntries();
	void OnPropertySelectionChanged(TSharedPtr<FDiffEntry> Item, ESelectInfo::Type SelectInfo);
	/** Prevents the hierarchy and summary lists from re-entering each other's selection handlers. */
	bool bSyncingSelection = false;

	void RebuildHierarchy();
	TSharedRef<ITableRow> OnGenerateHierarchyRow(TSharedPtr<FUnrealGitWidgetNode> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnHierarchySelectionChanged(TSharedPtr<FUnrealGitWidgetNode> Item, ESelectInfo::Type SelectInfo);
	TSharedRef<ITableRow> OnGeneratePropertyRow(TSharedPtr<FDiffEntry> Item, const TSharedRef<STableViewBase>& OwnerTable);
};
