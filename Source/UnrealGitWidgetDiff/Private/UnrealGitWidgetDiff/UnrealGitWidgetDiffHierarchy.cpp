// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffHierarchy.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "DiffUtils.h"
#include "WidgetBlueprint.h"

namespace
{
	void CollectWidgets(UWidget* Widget, int32 Depth, int32 ParentIndex, TArray<FUnrealGitWidgetNode>& OutNodes, TArray<UWidget*>& OutWidgets)
	{
		if (!Widget)
		{
			return;
		}

		FUnrealGitWidgetNode Node;
		Node.Name = Widget->GetName();
		Node.TypeName = Widget->GetClass()->GetName();
		Node.Depth = Depth;
		Node.ParentIndex = ParentIndex;

		const int32 Index = OutNodes.Add(Node);
		OutWidgets.Add(Widget);

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
			{
				CollectWidgets(Panel->GetChildAt(ChildIndex), Depth + 1, Index, OutNodes, OutWidgets);
			}
		}
	}

	void CollectBlueprint(UWidgetBlueprint* Blueprint, TArray<FUnrealGitWidgetNode>& OutNodes, TArray<UWidget*>& OutWidgets)
	{
		if (!Blueprint || !Blueprint->WidgetTree)
		{
			return;
		}
		CollectWidgets(Blueprint->WidgetTree->RootWidget, 0, INDEX_NONE, OutNodes, OutWidgets);
	}
}

TArray<FUnrealGitWidgetNode> FUnrealGitWidgetDiffHierarchy::Build(UWidgetBlueprint* Left, UWidgetBlueprint* Right)
{
	TArray<FUnrealGitWidgetNode> LeftNodes;
	TArray<UWidget*> LeftWidgets;
	CollectBlueprint(Left, LeftNodes, LeftWidgets);

	TArray<FUnrealGitWidgetNode> RightNodes;
	TArray<UWidget*> RightWidgets;
	CollectBlueprint(Right, RightNodes, RightWidgets);

	TMap<FString, int32> RightIndexByName;
	for (int32 Index = 0; Index < RightNodes.Num(); ++Index)
	{
		RightIndexByName.Add(RightNodes[Index].Name, Index);
	}

	TSet<FString> MatchedRightNames;
	TArray<FUnrealGitWidgetNode> Result;

	// Start from the left (older) tree order.
	for (int32 LeftIndex = 0; LeftIndex < LeftNodes.Num(); ++LeftIndex)
	{
		FUnrealGitWidgetNode Node = LeftNodes[LeftIndex];
		Node.LeftIndex = LeftIndex;

		if (const int32* RightIndexPtr = RightIndexByName.Find(Node.Name))
		{
			const int32 RightIndex = *RightIndexPtr;
			Node.RightIndex = RightIndex;
			MatchedRightNames.Add(Node.Name);

			if (LeftWidgets.IsValidIndex(LeftIndex) && RightWidgets.IsValidIndex(RightIndex))
			{
				TArray<FSingleObjectDiffEntry> Differences;
				DiffUtils::CompareUnrelatedObjects(LeftWidgets[LeftIndex], RightWidgets[RightIndex], Differences);
				if (Differences.Num() > 0)
				{
					Node.Change = EUnrealGitWidgetChange::Modified;
					for (const FSingleObjectDiffEntry& Difference : Differences)
					{
						Node.ChangedProperties.Add(Difference.Identifier.ToDisplayName());
					}
				}
			}
		}
		else
		{
			Node.Change = EUnrealGitWidgetChange::Removed;
		}

		Result.Add(MoveTemp(Node));
	}

	// Append widgets that only exist in the right (newer) tree.
	for (int32 RightIndex = 0; RightIndex < RightNodes.Num(); ++RightIndex)
	{
		if (MatchedRightNames.Contains(RightNodes[RightIndex].Name))
		{
			continue;
		}

		FUnrealGitWidgetNode Node = RightNodes[RightIndex];
		Node.RightIndex = RightIndex;
		Node.Change = EUnrealGitWidgetChange::Added;
		Result.Add(MoveTemp(Node));
	}

	return Result;
}
