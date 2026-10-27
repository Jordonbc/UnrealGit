// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffSession.h"

/**
 * @class SUnrealGitWidgetDiffView
 * Top-level tab view for a widget diff session: toolbar, viewer, hierarchy and summary.
 */
class SUnrealGitWidgetDiffView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUnrealGitWidgetDiffView) {}
		SLATE_ARGUMENT(TSharedPtr<FUnrealGitWidgetDiffSession>, Session)
	SLATE_END_ARGS()

	SUnrealGitWidgetDiffView();
	virtual ~SUnrealGitWidgetDiffView() override;

	/** Builds the widget from the supplied arguments. @param InArgs Slate construction arguments. */
	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/**
	 * @enum EViewMode
	 * Viewer layout mode for the diff tab.
	 */
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

	/** Creates live preview widgets for both revisions if not already built. */
	void EnsureLivePreviews();
	/**
	 * Selects the hierarchy node matching the given widget name.
	 * @param Name Widget name to select.
	 */
	void SelectWidgetByName(const FString& Name);

	/** @return Toolbar widget for the view. */
	TSharedRef<SWidget> BuildToolbar();
	/** @return Onion-blend slider row widget. */
	TSharedRef<SWidget> BuildOnionBlendRow();
	/** @return Viewer widget hosting the canvases. */
	TSharedRef<SWidget> BuildViewer();
	/** @return Hierarchy panel widget. */
	TSharedRef<SWidget> BuildHierarchyPanel();
	/** @return Summary panel widget. */
	TSharedRef<SWidget> BuildSummaryPanel();
	/** @return Property-difference inspector panel widget. */
	TSharedRef<SWidget> BuildPropertyInspector();
	/** Binds the session's update delegate to the view. */
	void WireSession();

	bool bFitToWindow = true;
	bool bSwapped = false;
	float OnionBlendPercent = 50.0f;

	FDelegateHandle OnUpdatedHandle;

	/** @return Brush wrapping the left texture. */
	const FSlateBrush* GetLeftBrush() const { return &LeftBrush; }
	/** @return Brush wrapping the right texture. */
	const FSlateBrush* GetRightBrush() const { return &RightBrush; }
	/** @return Brush wrapping the diff texture. */
	const FSlateBrush* GetDiffBrush() const { return &DiffBrush; }
	/** @return Brush wrapping the highlight texture. */
	const FSlateBrush* GetHighlightBrush() const { return &HighlightBrush; }

	/** @return true when the viewer is in onion-blend mode. */
	bool IsOnionMode() const { return ViewMode == EViewMode::Onion; }

	/** Handles session update notifications and refreshes the view. */
	void OnSessionUpdated();
	/** Requests a deferred refresh of the view. */
	void RequestRefresh();
	/** Updates all texture brushes from the session images. */
	void UpdateBrushes();
	/**
	 * Updates one brush from a texture.
	 * @param Brush Brush to update.
	 * @param Texture Texture to wrap, or null.
	 */
	void UpdateBrush(FSlateBrush& Brush, UTexture2D* Texture);

	/** @return Switcher index for the current view mode. */
	int32 GetSwitcherIndex() const;

	/**
	 * Handles view-mode combo selection changes.
	 * @param NewSelection Newly selected option.
	 * @param SelectInfo Selection change info.
	 */
	void OnModeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	/**
	 * Handles fit-to-window checkbox changes.
	 * @param NewState New checkbox state.
	 */
	void OnFitChanged(ECheckBoxState NewState);
	/** @return Reply for the swap-sides button. */
	FReply OnSwapClicked();
	/** @return Reply for the close button. */
	FReply OnCloseClicked();
	/**
	 * Handles onion-blend slider changes.
	 * @param NewValue New blend percentage.
	 */
	void OnOnionBlendChanged(float NewValue);

	/** @return Label for the left side. */
	FText GetLeftLabel() const;
	/** @return Label for the right side. */
	FText GetRightLabel() const;

	TSharedPtr<STreeView<TSharedPtr<FUnrealGitWidgetNode>>> HierarchyList;
	TArray<TSharedPtr<FUnrealGitWidgetNode>> HierarchyItems;
	TArray<TSharedPtr<FUnrealGitWidgetNode>> HierarchyRoots;

	/**
	 * Populates child nodes for a hierarchy row.
	 * @param Item Parent node.
	 * @param OutChildren Filled with child nodes.
	 */
	void OnGetHierarchyChildren(TSharedPtr<FUnrealGitWidgetNode> Item, TArray<TSharedPtr<FUnrealGitWidgetNode>>& OutChildren) const;
	/**
	 * Handles a hierarchy row click.
	 * @param Item Clicked node.
	 */
	void OnHierarchyRowClicked(TSharedPtr<FUnrealGitWidgetNode> Item);
	/**
	 * Focuses canvases on the given widget node.
	 * @param Node Node to focus.
	 */
	void FocusWidgetNode(const FUnrealGitWidgetNode& Node);

	/** Moves the selection to the next (+1) or previous (-1) changed widget and focuses it. */
	void NavigateChange(int32 Direction);
	/** Expands every ancestor of the node so the hierarchy can scroll to it. */
	void ExpandAncestors(const FUnrealGitWidgetNode& Node);
	/**
	 * Finds a hierarchy node by name.
	 * @param Name Node name.
	 * @return Matching node, or null.
	 */
	TSharedPtr<FUnrealGitWidgetNode> FindHierarchyNode(const FString& Name) const;
	/** True when the diff contains at least one changed widget to navigate to. @return True when changes exist. */
	bool HasChanges() const;
	/**
	 * Describes the selected change's position within the navigable change list.
	 * @return Text such as "Change 3 of 17"; a placeholder when nothing is selected or there are no changes.
	 */
	FText GetChangeCounterText() const;
	/**
	 * @struct FDiffEntry
	 * One row in the changed-properties summary list.
	 */
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

	/** Rows shown in the property-difference inspector for the selected widget. */
	TArray<TSharedPtr<FUnrealGitPropertyDiff>> InspectorItems;
	TSharedPtr<SListView<TSharedPtr<FUnrealGitPropertyDiff>>> InspectorList;
	/** Name of the widget currently shown in the inspector, or empty when nothing is selected. */
	FString InspectorWidgetName;
	/** Explanation shown when the inspector has no rows. */
	FText InspectorEmptyMessage;

	bool bShowAdded = true;
	bool bShowRemoved = true;
	bool bShowModified = true;

	/** Rebuilds the changed-properties summary entries. */
	void RebuildDiffEntries();
	/**
	 * Handles property-list selection changes.
	 * @param Item Selected entry.
	 * @param SelectInfo Selection change info.
	 */
	void OnPropertySelectionChanged(TSharedPtr<FDiffEntry> Item, ESelectInfo::Type SelectInfo);
	/** Prevents the hierarchy and summary lists from re-entering each other's selection handlers. */
	bool bSyncingSelection = false;

	/** Rebuilds the hierarchy tree from the session result. */
	void RebuildHierarchy();
	/**
	 * Generates a hierarchy row widget.
	 * @param Item Node to render.
	 * @param OwnerTable Owning table.
	 * @return Row widget.
	 */
	TSharedRef<ITableRow> OnGenerateHierarchyRow(TSharedPtr<FUnrealGitWidgetNode> Item, const TSharedRef<STableViewBase>& OwnerTable);
	/**
	 * Handles hierarchy selection changes.
	 * @param Item Selected node.
	 * @param SelectInfo Selection change info.
	 */
	void OnHierarchySelectionChanged(TSharedPtr<FUnrealGitWidgetNode> Item, ESelectInfo::Type SelectInfo);
	/**
	 * Generates a property-list row widget.
	 * @param Item Entry to render.
	 * @param OwnerTable Owning table.
	 * @return Row widget.
	 */
	TSharedRef<ITableRow> OnGeneratePropertyRow(TSharedPtr<FDiffEntry> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/**
	 * Rebuilds the inspector rows for the given node and updates the empty state.
	 * @param Node Selected node, or null when nothing is selected.
	 */
	void RefreshInspector(const FUnrealGitWidgetNode* Node);
	/** @return Inspector header text for the current selection. */
	FText GetInspectorHeaderText() const;
	/** @return Visible when the inspector has rows to display. */
	EVisibility GetInspectorListVisibility() const;
	/**
	 * Generates one inspector row.
	 * @param Item Difference to render.
	 * @param OwnerTable Owning table.
	 * @return Row widget.
	 */
	TSharedRef<ITableRow> OnGenerateInspectorRow(TSharedPtr<FUnrealGitPropertyDiff> Item, const TSharedRef<STableViewBase>& OwnerTable);
	/**
	 * Opens the copy context menu for a difference row.
	 * @param Item Difference under the cursor.
	 * @param ScreenPosition Menu position in screen space.
	 */
	void ShowInspectorContextMenu(TSharedPtr<FUnrealGitPropertyDiff> Item, const FVector2D& ScreenPosition);
};
