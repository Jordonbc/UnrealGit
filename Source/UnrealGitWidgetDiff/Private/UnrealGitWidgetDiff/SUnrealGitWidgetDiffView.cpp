// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/SUnrealGitWidgetDiffView.h"

#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffSession.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffTab.h"
#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffLauncher.h"

#include "Engine/Texture2D.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "UnrealGitWidgetDiffView"

SUnrealGitWidgetDiffView::SUnrealGitWidgetDiffView() {}

SUnrealGitWidgetDiffView::~SUnrealGitWidgetDiffView()
{
	if (Session.IsValid())
	{
		Session->OnUpdated.Remove(OnUpdatedHandle);
	}
}

void SUnrealGitWidgetDiffView::Construct(const FArguments& InArgs)
{
	Session = InArgs._Session;

	LeftBrush.DrawAs = ESlateBrushDrawType::Image;
	RightBrush.DrawAs = ESlateBrushDrawType::Image;
	DiffBrush.DrawAs = ESlateBrushDrawType::Image;

	ModeOptions.Add(MakeShared<FString>(TEXT("Side by Side")));
	ModeOptions.Add(MakeShared<FString>(TEXT("Difference")));
	ModeOptions.Add(MakeShared<FString>(TEXT("Onion")));
	CurrentModeOption = ModeOptions[0];

	PreviewModeOptions.Add(MakeShared<FString>(TEXT("Match Designer")));
	PreviewModeOptions.Add(MakeShared<FString>(TEXT("Fill Screen")));
	PreviewModeOptions.Add(MakeShared<FString>(TEXT("Desired")));
	PreviewModeOptions.Add(MakeShared<FString>(TEXT("Custom 800x600")));
	CurrentPreviewModeOption = PreviewModeOptions[0];

	if (Session.IsValid())
	{
		OnUpdatedHandle = Session->OnUpdated.AddRaw(this, &SUnrealGitWidgetDiffView::OnSessionUpdated);
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[
				SAssignNew(ModeComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&ModeOptions)
				.InitiallySelectedItem(CurrentModeOption)
				.OnSelectionChanged(this, &SUnrealGitWidgetDiffView::OnModeChanged)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
				{
					return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
				})
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return CurrentModeOption.IsValid() ? FText::FromString(*CurrentModeOption) : FText::GetEmpty();
					})
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[
				SAssignNew(PreviewModeComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&PreviewModeOptions)
				.InitiallySelectedItem(CurrentPreviewModeOption)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> NewSelection, ESelectInfo::Type)
				{
					if (!NewSelection.IsValid()) return;
					CurrentPreviewModeOption = NewSelection;
					EThumbnailPreviewSizeMode Mode = EThumbnailPreviewSizeMode::MatchDesignerMode;
					if (*NewSelection == TEXT("Fill Screen")) Mode = EThumbnailPreviewSizeMode::FillScreen;
					else if (*NewSelection == TEXT("Desired")) Mode = EThumbnailPreviewSizeMode::Desired;
					else if (*NewSelection == TEXT("Custom 800x600")) Mode = EThumbnailPreviewSizeMode::Custom;
					if (Session.IsValid())
					{
						FUnrealGitWidgetDiffLauncher::Launch(Session->AssetFileName, Session->LeftRevision, Session->RightRevision, Mode);
						FUnrealGitWidgetDiffTab::CloseActive();
					}
				})
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
				{
					return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
				})
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return CurrentPreviewModeOption.IsValid() ? FText::FromString(*CurrentPreviewModeOption) : FText::GetEmpty();
					})
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bFitToWindow ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged(this, &SUnrealGitWidgetDiffView::OnFitChanged)
				[ SNew(STextBlock).Text(LOCTEXT("FitToWindow", "Fit to Window")) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[
				SNew(SButton).OnClicked(this, &SUnrealGitWidgetDiffView::OnSwapClicked)
				[ SNew(SImage).Image(FAppStyle::GetBrush(TEXT("Icons.SortUpDown"))) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[
				SNew(SButton).OnClicked(this, &SUnrealGitWidgetDiffView::OnCloseClicked)
				[ SNew(SImage).Image(FAppStyle::GetBrush(TEXT("Icons.Close"))) ]
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]() { return IsOnionMode() ? EVisibility::Visible : EVisibility::Collapsed; })
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[ SNew(STextBlock).Text(LOCTEXT("BlendLabel", "Blend")) ]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(SSlider)
				.MinValue(0.0f).MaxValue(100.0f)
				.Value_Lambda([this]() { return OnionBlendPercent; })
				.OnValueChanged(this, &SUnrealGitWidgetDiffView::OnOnionBlendChanged)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f)
			[ SNew(STextBlock).Text_Lambda([this]() { return FText::Format(LOCTEXT("BlendValue", "{0}%"), FText::AsNumber(FMath::RoundToInt(OnionBlendPercent))); }) ]
		]

		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SHorizontalBox)
			// Left: read-only hierarchy tree.
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(260.0f)
				[
					SNew(SBorder).Padding(4.0f)
					[
						SAssignNew(HierarchyList, SListView<TSharedPtr<FUnrealGitWidgetNode>>)
						.ListItemsSource(&HierarchyItems)
						.OnGenerateRow(this, &SUnrealGitWidgetDiffView::OnGenerateHierarchyRow)
						.OnSelectionChanged(this, &SUnrealGitWidgetDiffView::OnHierarchySelectionChanged)
						.SelectionMode(ESelectionMode::Single)
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[ SNew(SSeparator).Orientation(Orient_Vertical) ]
			// Center: dual preview canvases.
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SScrollBox)
				.Orientation(Orient_Vertical)
				+ SScrollBox::Slot()
				[
					SAssignNew(Switcher, SWidgetSwitcher)
					.WidgetIndex_Lambda([this]() { return GetSwitcherIndex(); })

					+ SWidgetSwitcher::Slot()
					[ SNew(STextBlock).Text(LOCTEXT("Loading", "Loading...")).Justification(ETextJustify::Center) ]

					+ SWidgetSwitcher::Slot()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							if (Session.IsValid())
							{
								if (!Session->StatusMessage.IsEmpty()) return Session->StatusMessage;
								if (!Session->Result.Error.IsEmpty()) return Session->Result.Error;
							}
							return LOCTEXT("Error", "Error");
						})
						.Justification(ETextJustify::Center)
					]

					+ SWidgetSwitcher::Slot()
					[ SNew(STextBlock).Text(LOCTEXT("Empty", "No diff available.")).Justification(ETextJustify::Center) ]

					+ SWidgetSwitcher::Slot()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
							[ SNew(STextBlock).Text_Lambda([this]() { return GetLeftLabel(); }) ]
							+ SVerticalBox::Slot().FillHeight(1.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
							[ SNew(SImage).Image(this, &SUnrealGitWidgetDiffView::GetLeftBrush) ]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
							[ SNew(STextBlock).Text_Lambda([this]() { return GetRightLabel(); }) ]
							+ SVerticalBox::Slot().FillHeight(1.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
							[ SNew(SImage).Image(this, &SUnrealGitWidgetDiffView::GetRightBrush) ]
						]
					]

					+ SWidgetSwitcher::Slot()
					[ SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
						[ SNew(SImage).Image(this, &SUnrealGitWidgetDiffView::GetDiffBrush) ] ]

					+ SWidgetSwitcher::Slot()
					[ SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
						[ SNew(SImage).Image(this, &SUnrealGitWidgetDiffView::GetRightBrush)
							.ColorAndOpacity_Lambda([this]() { return FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, OnionBlendPercent / 100.0f)); }) ] ]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[ SNew(SSeparator).Orientation(Orient_Vertical) ]
			// Right: read-only changed-properties list.
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(300.0f)
				[
					SNew(SBorder).Padding(4.0f)
					[
						SAssignNew(PropertyList, SListView<TSharedPtr<FString>>)
						.ListItemsSource(&PropertyItems)
						.OnGenerateRow(this, &SUnrealGitWidgetDiffView::OnGeneratePropertyRow)
					]
				]
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			SNew(SBorder).Padding(4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNew(STextBlock).Text_Lambda([this]() { return GetLeftLabel(); }) ]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNew(STextBlock).Text_Lambda([this]() { return GetRightLabel(); }) ]
				+ SHorizontalBox::Slot().FillWidth(1.0f)[ SNew(STextBlock).Text_Lambda([this]() { return GetSummaryText(); }) ]
			]
		]
		]
	];

	UpdateBrushes();
	RebuildHierarchy();
	if (Session.IsValid())
	{
		OnUpdatedHandle = Session->OnUpdated.AddRaw(this, &SUnrealGitWidgetDiffView::OnSessionUpdated);
	}
}

void SUnrealGitWidgetDiffView::OnSessionUpdated()
{
	RequestRefresh();
}

void SUnrealGitWidgetDiffView::RequestRefresh()
{
	UpdateBrushes();
	RebuildHierarchy();
	if (Switcher.IsValid())
	{
		Switcher->SetActiveWidgetIndex(GetSwitcherIndex());
	}
	Invalidate(EInvalidateWidget::Layout);
}

void SUnrealGitWidgetDiffView::UpdateBrushes()
{
	UTexture2D* LeftTex = nullptr;
	UTexture2D* RightTex = nullptr;
	UTexture2D* DiffTex = nullptr;

	if (Session.IsValid() && Session->Images.IsValid())
	{
		LeftTex = Session->Images->LeftTexture;
		RightTex = Session->Images->RightTexture;
		DiffTex = Session->Images->DiffTexture;
	}

	if (bSwapped)
	{
		UTexture2D* Tmp = LeftTex;
		LeftTex = RightTex;
		RightTex = Tmp;
	}

	UpdateBrush(LeftBrush, LeftTex);
	UpdateBrush(RightBrush, RightTex);
	UpdateBrush(DiffBrush, DiffTex);
}

void SUnrealGitWidgetDiffView::UpdateBrush(FSlateBrush& Brush, UTexture2D* Texture)
{
	Brush.SetResourceObject(Texture);
	if (!Texture)
	{
		Brush.ImageSize = FVector2D(0.0f, 0.0f);
		return;
	}

	FVector2D Size((float)Texture->GetSizeX(), (float)Texture->GetSizeY());
	if (bFitToWindow)
	{
		const float Fit = FMath::Min(1.0f, FMath::Min(1200.0f / FMath::Max(1.0f, Size.X), 800.0f / FMath::Max(1.0f, Size.Y)));
		Size *= Fit;
	}
	Brush.ImageSize = Size;
}

void SUnrealGitWidgetDiffView::RebuildHierarchy()
{
	if (!Session.IsValid() || Session->State != EUnrealGitWidgetDiffState::Ready)
	{
		return;
	}

	HierarchyItems.Reset();
	for (const FUnrealGitWidgetNode& Node : Session->Result.Hierarchy)
	{
		HierarchyItems.Add(MakeShared<FUnrealGitWidgetNode>(Node));
	}

	if (HierarchyList.IsValid())
	{
		HierarchyList->RequestListRefresh();
	}
	if (PropertyList.IsValid())
	{
		PropertyList->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SUnrealGitWidgetDiffView::OnGenerateHierarchyRow(TSharedPtr<FUnrealGitWidgetNode> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	FLinearColor Color(0.7f, 0.7f, 0.7f, 1.0f);
	if (Item.IsValid())
	{
		switch (Item->Change)
		{
		case EUnrealGitWidgetChange::Added: Color = FLinearColor(0.3f, 0.9f, 0.3f, 1.0f); break;
		case EUnrealGitWidgetChange::Removed: Color = FLinearColor(1.0f, 0.3f, 0.3f, 1.0f); break;
		case EUnrealGitWidgetChange::Modified: Color = FLinearColor(1.0f, 0.8f, 0.2f, 1.0f); break;
		default: Color = FLinearColor(0.75f, 0.75f, 0.75f, 1.0f); break;
		}
	}

	const float Indent = Item.IsValid() ? Item->Depth * 12.0f : 0.0f;
	return SNew(STableRow<TSharedPtr<FUnrealGitWidgetNode>>, OwnerTable)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(SSpacer).Size(FVector2D(Indent, 1.0f)) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->Name : FString())).ColorAndOpacity(FSlateColor(Color)) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 0.0f, 0.0f)
		[ SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? Item->TypeName : FString())).ColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.45f, 0.45f, 1.0f))) ]
	];
}

void SUnrealGitWidgetDiffView::OnHierarchySelectionChanged(TSharedPtr<FUnrealGitWidgetNode> Item, ESelectInfo::Type /*SelectInfo*/)
{
	PropertyItems.Reset();
	if (Item.IsValid())
	{
		for (const FString& Property : Item->ChangedProperties)
		{
			PropertyItems.Add(MakeShared<FString>(Property));
		}
	}
	if (PropertyList.IsValid())
	{
		PropertyList->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SUnrealGitWidgetDiffView::OnGeneratePropertyRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Item.IsValid() ? *Item : FString()))
		.ColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f)))
	];
}

int32 SUnrealGitWidgetDiffView::GetSwitcherIndex() const
{
	if (Session.IsValid())
	{
		switch (Session->State)
		{
		case EUnrealGitWidgetDiffState::Loading: return 0;
		case EUnrealGitWidgetDiffState::Error: return 1;
		case EUnrealGitWidgetDiffState::Empty: return 2;
		case EUnrealGitWidgetDiffState::Ready:
			switch (ViewMode)
			{
			case EViewMode::SideBySide: return 3;
			case EViewMode::Difference: return 4;
			case EViewMode::Onion: return 5;
			}
			break;
		}
	}
	return 0;
}

void SUnrealGitWidgetDiffView::OnModeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type /*SelectInfo*/)
{
	if (!NewSelection.IsValid()) return;
	CurrentModeOption = NewSelection;
	if (*NewSelection == TEXT("Side by Side")) ViewMode = EViewMode::SideBySide;
	else if (*NewSelection == TEXT("Difference")) ViewMode = EViewMode::Difference;
	else if (*NewSelection == TEXT("Onion")) ViewMode = EViewMode::Onion;
	RequestRefresh();
}

void SUnrealGitWidgetDiffView::OnFitChanged(ECheckBoxState NewState)
{
	bFitToWindow = (NewState == ECheckBoxState::Checked);
	RequestRefresh();
}

FReply SUnrealGitWidgetDiffView::OnSwapClicked()
{
	bSwapped = !bSwapped;
	RequestRefresh();
	return FReply::Handled();
}

FReply SUnrealGitWidgetDiffView::OnCloseClicked()
{
	FUnrealGitWidgetDiffTab::CloseActive();
	return FReply::Handled();
}

void SUnrealGitWidgetDiffView::OnOnionBlendChanged(float NewValue)
{
	OnionBlendPercent = NewValue;
	RequestRefresh();
}

FText SUnrealGitWidgetDiffView::GetLeftLabel() const
{
	if (Session.IsValid())
	{
		return bSwapped ? Session->Result.Right.Label : Session->Result.Left.Label;
	}
	return FText::GetEmpty();
}

FText SUnrealGitWidgetDiffView::GetRightLabel() const
{
	if (Session.IsValid())
	{
		return bSwapped ? Session->Result.Left.Label : Session->Result.Right.Label;
	}
	return FText::GetEmpty();
}

FText SUnrealGitWidgetDiffView::GetSummaryText() const
{
	if (Session.IsValid())
	{
		const FUnrealGitWidgetDiffResult& R = Session->Result;
		if (R.DiffPixelCount == 0)
		{
			return LOCTEXT("NoDiff", "No visual differences detected.");
		}
		return FText::Format(LOCTEXT("Summary", "Diff pixels: {0} ({1}) Canvas: {2}x{3}"),
			FText::AsNumber(R.DiffPixelCount),
			FText::AsPercent(R.DiffRatio),
			FText::AsNumber(R.CanvasSize.X),
			FText::AsNumber(R.CanvasSize.Y));
	}
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
