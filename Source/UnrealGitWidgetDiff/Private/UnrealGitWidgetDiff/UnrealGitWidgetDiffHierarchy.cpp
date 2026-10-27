// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGitWidgetDiff/Private/UnrealGitWidgetDiff/UnrealGitWidgetDiffHierarchy.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "DiffUtils.h"
#include "WidgetBlueprint.h"

namespace
{
	void CollectWidgets(UWidget* Widget, UWidget* Parent, int32 Depth, const FString& ParentPath, TArray<FUnrealGitWidgetNode>& OutNodes, TArray<UWidget*>& OutWidgets)
	{
		if (!Widget)
		{
			return;
		}

		FUnrealGitWidgetNode Node;
		Node.Name = Widget->GetName();
		Node.ParentName = Parent ? Parent->GetName() : FString();
		Node.Path = ParentPath.IsEmpty() ? Node.Name : (ParentPath + TEXT("/") + Node.Name);
		Node.TypeName = Widget->GetClass()->GetName();
		Node.Depth = Depth;
		Node.ParentIndex = INDEX_NONE;

		const int32 Index = OutNodes.Add(Node);
		OutWidgets.Add(Widget);

		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 ChildIndex = 0; ChildIndex < Panel->GetChildrenCount(); ++ChildIndex)
			{
				CollectWidgets(Panel->GetChildAt(ChildIndex), Widget, Depth + 1, Node.Path, OutNodes, OutWidgets);
			}
		}
	}

	void CollectBlueprint(UWidgetBlueprint* Blueprint, TArray<FUnrealGitWidgetNode>& OutNodes, TArray<UWidget*>& OutWidgets)
	{
		if (!Blueprint || !Blueprint->WidgetTree)
		{
			return;
		}
		CollectWidgets(Blueprint->WidgetTree->RootWidget, nullptr, 0, FString(), OutNodes, OutWidgets);

		// Fallback: some diff-loaded packages expose the full widget list but not
		// child slots through the panel API. Rebuild from GetAllWidgets in that case.
		if (OutNodes.Num() <= 1)
		{
			TArray<UWidget*> AllWidgets;
			Blueprint->WidgetTree->GetAllWidgets(AllWidgets);
			if (AllWidgets.Num() > OutNodes.Num())
			{
				OutNodes.Reset();
				OutWidgets.Reset();

				for (UWidget* Widget : AllWidgets)
				{
					if (!Widget)
					{
						continue;
					}

					FUnrealGitWidgetNode Node;
					Node.Name = Widget->GetName();
					Node.TypeName = Widget->GetClass()->GetName();
					Node.ParentName = Widget->GetParent() ? Widget->GetParent()->GetName() : FString();

					FString Path = Node.Name;
					int32 Depth = 0;
					for (UWidget* Ancestor = Widget->GetParent(); Ancestor; Ancestor = Ancestor->GetParent())
					{
						Path = Ancestor->GetName() + TEXT("/") + Path;
						++Depth;
					}
					Node.Path = Path;
					Node.Depth = Depth;

					OutNodes.Add(Node);
					OutWidgets.Add(Widget);
				}
			}
		}
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

	TMap<FString, int32> LeftByPath;
	TMap<FString, int32> RightByPath;
	TMap<FString, int32> LeftByName;
	TMap<FString, int32> RightByName;
	for (int32 Index = 0; Index < LeftNodes.Num(); ++Index)
	{
		LeftByPath.Add(LeftNodes[Index].Path, Index);
		LeftByName.Add(LeftNodes[Index].Name, Index);
	}
	for (int32 Index = 0; Index < RightNodes.Num(); ++Index)
	{
		RightByPath.Add(RightNodes[Index].Path, Index);
		RightByName.Add(RightNodes[Index].Name, Index);
	}

	TSet<int32> MatchedRightIndices;
	TArray<FUnrealGitWidgetNode> Result;

	// Start from the left (older) tree order.
	for (int32 LeftIndex = 0; LeftIndex < LeftNodes.Num(); ++LeftIndex)
	{
		FUnrealGitWidgetNode Node = LeftNodes[LeftIndex];
		Node.LeftIndex = LeftIndex;

		int32 RightIndex = INDEX_NONE;
		bool bMatchedByPath = false;

		if (const int32* PathMatch = RightByPath.Find(Node.Path))
		{
			RightIndex = *PathMatch;
			bMatchedByPath = true;
		}
		else if (const int32* NameMatch = RightByName.Find(Node.Name))
		{
			// Same widget name, different path: reparented or an ancestor was renamed.
			RightIndex = *NameMatch;
		}

		if (RightIndex != INDEX_NONE)
		{
			Node.RightIndex = RightIndex;
			MatchedRightIndices.Add(RightIndex);

			if (LeftWidgets.IsValidIndex(LeftIndex) && RightWidgets.IsValidIndex(RightIndex))
			{
				TArray<FSingleObjectDiffEntry> Differences;
				DiffUtils::CompareUnrelatedObjects(LeftWidgets[LeftIndex], RightWidgets[RightIndex], Differences);
				for (const FSingleObjectDiffEntry& Difference : Differences)
				{
					Node.ChangedProperties.Add(Difference.Identifier.ToDisplayName());
				}
			}

			if (!bMatchedByPath)
			{
				Node.ChangedProperties.AddUnique(TEXT("Reparented / renamed ancestor"));
			}

			Node.Change = Node.ChangedProperties.Num() > 0
				? EUnrealGitWidgetChange::Modified
				: EUnrealGitWidgetChange::Unchanged;
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
		if (MatchedRightIndices.Contains(RightIndex))
		{
			continue;
		}

		FUnrealGitWidgetNode Node = RightNodes[RightIndex];
		Node.RightIndex = RightIndex;
		Node.Change = EUnrealGitWidgetChange::Added;
		Result.Add(MoveTemp(Node));
	}

	// Mark added widgets whose ancestor was also added, so the summary can
	// explain that a whole new container was introduced rather than treating
	// every descendant as an unrelated top-level change.
	TSet<FString> AddedNames;
	for (const FUnrealGitWidgetNode& Node : Result)
	{
		if (Node.Change == EUnrealGitWidgetChange::Added)
		{
			AddedNames.Add(Node.Name);
		}
	}
	for (FUnrealGitWidgetNode& Node : Result)
	{
		if (Node.Change != EUnrealGitWidgetChange::Added)
		{
			continue;
		}
		FString ParentName = Node.ParentName;
		while (!ParentName.IsEmpty())
		{
			if (AddedNames.Contains(ParentName))
			{
				Node.bIsChildOfChange = true;
				Node.ChangedAncestor = ParentName;
				break;
			}
			const FUnrealGitWidgetNode* ParentNode = Result.FindByPredicate([&ParentName](const FUnrealGitWidgetNode& Candidate)
			{
				return Candidate.Name == ParentName;
			});
			ParentName = ParentNode ? ParentNode->ParentName : FString();
		}
	}

	return Result;
}
