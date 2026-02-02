// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/Settings/UnrealGitSettingsResolver.h"

#include "HAL/FileManager.h"
#include "UnrealGit/Settings/UnrealGitProjectSettings.h"
#include "UnrealGit/Settings/UnrealGitUserSettings.h"

static bool ValidateDirectoryOrEmpty(const FString& Directory, FText& OutError)
{
	if (Directory.IsEmpty())
	{
		return true;
	}

	if (!IFileManager::Get().DirectoryExists(*Directory))
	{
		OutError = FText::FromString(FString::Printf(TEXT("Directory does not exist: %s"), *Directory));
		return false;
	}

	return true;
}

static bool ValidateFileOrEmpty(const FString& FilePath, FText& OutError)
{
	if (FilePath.IsEmpty())
	{
		return true;
	}

	if (!IFileManager::Get().FileExists(*FilePath))
	{
		OutError = FText::FromString(FString::Printf(TEXT("File does not exist: %s"), *FilePath));
		return false;
	}

	return true;
}

bool FUnrealGitSettingsResolver::Build(FUnrealGitProviderSettings& OutSettings, FText& OutErrorText)
{
	OutSettings = FUnrealGitProviderSettings();
	OutErrorText = FText::GetEmpty();

	const UUnrealGitProjectSettings* Project = GetDefault<UUnrealGitProjectSettings>();
	const UUnrealGitUserSettings* User = GetDefault<UUnrealGitUserSettings>();

	OutSettings.GitExecutable = User && !User->GitExecutableOverride.FilePath.IsEmpty() ? User->GitExecutableOverride.FilePath : FString(TEXT("git"));
	OutSettings.RepositoryDiscoveryDirectory = Project && !Project->RepositoryRootOverride.Path.IsEmpty()
		? Project->RepositoryRootOverride.Path
		: FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	OutSettings.bPullRebase = Project ? Project->bPullRebase : false;
	OutSettings.bAutoPushAfterSubmit = Project ? Project->bAutoPushAfterSubmit : false;
	OutSettings.bEnableLfsLocks = Project ? Project->bEnableLfsLocks : true;
	OutSettings.LfsLocksRefreshIntervalSeconds = Project ? Project->LfsLocksRefreshIntervalSeconds : 15;
	OutSettings.bAutoLockOnCheckout = User ? User->bAutoLockOnCheckout : false;

	FText Error;
	if (!ValidateFileOrEmpty(OutSettings.GitExecutable, Error))
	{
		OutErrorText = Error;
		return false;
	}
	if (!ValidateDirectoryOrEmpty(OutSettings.RepositoryDiscoveryDirectory, Error))
	{
		OutErrorText = Error;
		return false;
	}

	return true;
}

