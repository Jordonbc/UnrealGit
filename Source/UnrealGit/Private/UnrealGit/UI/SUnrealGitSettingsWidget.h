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

	/** Applies the manually entered Git executable path.
	 * @return Reply describing how the event was handled. */
	FReply OnSetGitPath();

	/** Opens a file dialog to choose the Git executable.
	 * @return Reply describing how the event was handled. */
	FReply OnBrowseGitPath();

	/** Initializes a new Git repository for the current project.
	 * @return Reply describing how the event was handled. */
	FReply OnInitialize();

	/** Toggles creation of a .gitignore file during initialization.
	 * @param NewCheckedState New checkbox state. */
	void OnCheckedGitIgnore(ECheckBoxState NewCheckedState);

	/** Toggles creation of a README file during initialization.
	 * @param NewCheckedState New checkbox state. */
	void OnCheckedReadme(ECheckBoxState NewCheckedState);

	/** Toggles creation of a .gitattributes file during initialization.
	 * @param NewCheckedState New checkbox state. */
	void OnCheckedGitAttributes(ECheckBoxState NewCheckedState);

	/** Toggles creation of an initial commit during initialization.
	 * @param NewCheckedState New checkbox state. */
	void OnCheckedInitialCommit(ECheckBoxState NewCheckedState);

	/** Returns the visibility of the initialization controls (Collapsed when initialization is unavailable). */
	EVisibility CanInitializeRepository() const;

	/** Wraps a status value for display.
	 * @param Value Status value to wrap.
	 * @return Wrapped status text. */
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