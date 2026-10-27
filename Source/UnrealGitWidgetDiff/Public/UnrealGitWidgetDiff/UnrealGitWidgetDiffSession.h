// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealGitWidgetDiff/UnrealGitWidgetDiffTypes.h"
#include "UnrealGitWidgetDiffSession.generated.h"

class UTexture2D;

/**
 * @class UUnrealGitWidgetDiffImages
 * Holds the rendered textures and live blueprints for a widget diff session.
 */
UCLASS(Transient)
class UUnrealGitWidgetDiffImages : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TObjectPtr<UTexture2D> LeftTexture = nullptr;

	UPROPERTY()
	TObjectPtr<UTexture2D> RightTexture = nullptr;

	UPROPERTY()
	TObjectPtr<UTexture2D> DiffTexture = nullptr;

	UPROPERTY()
	TObjectPtr<UTexture2D> HighlightTexture = nullptr;

	/** Live blueprints kept alive so the view can host their Slate widgets. */
	UPROPERTY()
	TObjectPtr<class UWidgetBlueprint> LeftBlueprint = nullptr;

	UPROPERTY()
	TObjectPtr<class UWidgetBlueprint> RightBlueprint = nullptr;
};

/**
 * @enum EUnrealGitWidgetDiffState
 * Loading, ready, error, or empty state of a widget diff session.
 */
enum class EUnrealGitWidgetDiffState : uint8
{
	Loading,
	Ready,
	Error,
	Empty,
};

/**
 * @class FUnrealGitWidgetDiffSession
 * Shared state for a single widget diff session. The tab UI binds to this.
 */
class FUnrealGitWidgetDiffSession : public TSharedFromThis<FUnrealGitWidgetDiffSession, ESPMode::ThreadSafe>
{
public:
	FString AssetFileName;
	FString LeftRevision;
	FString RightRevision;

	EUnrealGitWidgetDiffState State = EUnrealGitWidgetDiffState::Loading;
	FText StatusMessage;
	FUnrealGitWidgetDiffResult Result;
	TStrongObjectPtr<UUnrealGitWidgetDiffImages> Images;

	FSimpleMulticastDelegate OnUpdated;

	/**
	 * Stores the diff result and images and marks the session ready.
	 * @param InResult Diff result to store.
	 * @param InImages Rendered images to store.
	 */
	void SetReady(const FUnrealGitWidgetDiffResult& InResult, const TStrongObjectPtr<UUnrealGitWidgetDiffImages>& InImages);

	/**
	 * Stores an error message and marks the session in the error state.
	 * @param InMessage Error message to store.
	 */
	void SetError(const FText& InMessage);
};
