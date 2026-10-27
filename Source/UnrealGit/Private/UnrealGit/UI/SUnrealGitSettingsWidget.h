// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FUnrealGitSourceControlProvider;
class SEditableTextBox;
class SMultiLineEditableTextBox;
class SErrorText;
class SUniformGridPanel;
class SCheckBox;
class SButton;

enum class ECheckBoxState : uint8;

/**
 * @class SUnrealGitSettingsWidget
 * Slate widget for configuring the UnrealGit source control provider.
 *
 * Presents the current provider status and repository-initialization controls.
 */
class SUnrealGitSettingsWidget final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUnrealGitSettingsWidget) {}
		SLATE_ARGUMENT(FUnrealGitSourceControlProvider*, Provider)
	SLATE_END_ARGS()

	/** Builds the widget from the supplied arguments. */
	void Construct(const FArguments& InArgs);

private:
	/** Refreshes the displayed provider status. */
	void RefreshStatus();

	/** Applies the manually entered Git executable path. */
	FReply OnSetGitPath();

	/** Opens a file dialog to choose the Git executable. */
	FReply OnBrowseGitPath();

	/** Initializes a new Git repository for the current project. */
	FReply OnInitialize();

	/** Toggles creation of a .gitignore file during initialization. */
	void OnCheckedGitIgnore(ECheckBoxState NewCheckedState);

	/** Toggles creation of a README file during initialization. */
	void OnCheckedReadme(ECheckBoxState NewCheckedState);

	/** Toggles creation of a .gitattributes file during initialization. */
	void OnCheckedGitAttributes(ECheckBoxState NewCheckedState);

	/** Toggles creation of an initial commit during initialization. */
	void OnCheckedInitialCommit(ECheckBoxState NewCheckedState);

	/** Returns whether the repository can currently be initialized. */
	EVisibility CanInitializeRepository() const;

	/** Wraps a status value for display. */
	FText StatusValue(const FString& Value) const;

	FUnrealGitSourceControlProvider* Provider = nullptr;
	TSharedPtr<SEditableTextBox> GitPathBox;
	TSharedPtr<SMultiLineEditableTextBox> OriginBox;
	TSharedPtr<SErrorText> ErrorText;
	TSharedPtr<SUniformGridPanel> StatusGrid;

	bool bCreateGitIgnore = true;
	bool bCreateReadme = true;
	bool bCreateGitAttributes = false;
	bool bCreateInitialCommit = true;
	bool bInitializing = false;
};