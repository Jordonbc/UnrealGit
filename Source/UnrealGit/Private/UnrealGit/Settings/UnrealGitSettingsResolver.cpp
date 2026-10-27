// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/Settings/UnrealGitSettingsResolver.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "UnrealGit/Private/UnrealGit/Git/GitBinaryFinder.h"
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

static bool IsExecutableOnPath(const FString& FilePath)
{
	if (FilePath.IsEmpty() || !FPaths::IsRelative(FilePath))
	{
		return false;
	}

	if (FilePath.Contains(TEXT("/")) || FilePath.Contains(TEXT("\\")))
	{
		return false;
	}

	const FString PathVar = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
	if (PathVar.IsEmpty())
	{
		return false;
	}

	TArray<FString> PathDirs;
#if PLATFORM_WINDOWS
	const TCHAR* PathDelimiter = TEXT(";");
#else
	const TCHAR* PathDelimiter = TEXT(":");
#endif
	PathVar.ParseIntoArray(PathDirs, PathDelimiter, true);

	const FString ExecutableName = FPaths::GetCleanFilename(FilePath);
	if (ExecutableName.IsEmpty())
	{
		return false;
	}

	for (const FString& Dir : PathDirs)
	{
		if (Dir.IsEmpty())
		{
			continue;
		}

		const FString Candidate = FPaths::Combine(Dir, ExecutableName);
		if (IFileManager::Get().FileExists(*Candidate))
		{
			return true;
		}

#if PLATFORM_WINDOWS
		if (FPaths::GetExtension(ExecutableName).IsEmpty())
		{
			const FString CandidateWithExe = Candidate + TEXT(".exe");
			if (IFileManager::Get().FileExists(*CandidateWithExe))
			{
				return true;
			}
		}
#endif
	}

	return false;
}

static bool ValidateFileOrEmpty(const FString& FilePath, FText& OutError)
{
	if (FilePath.IsEmpty())
	{
		return true;
	}

	if (IFileManager::Get().FileExists(*FilePath) || IsExecutableOnPath(FilePath))
	{
		return true;
	}

	OutError = FText::FromString(FString::Printf(TEXT("File does not exist or is not on PATH: %s"), *FilePath));
	return false;
}

bool FUnrealGitSettingsResolver::Build(FUnrealGitProviderSettings& OutSettings, FText& OutErrorText)
{
	OutSettings = FUnrealGitProviderSettings();
	OutErrorText = FText::GetEmpty();

	const UUnrealGitProjectSettings* Project = GetDefault<UUnrealGitProjectSettings>();
	const UUnrealGitUserSettings* User = GetDefault<UUnrealGitUserSettings>();

	FString GitExecutable = User && !User->GitExecutableOverride.FilePath.IsEmpty()
		? User->GitExecutableOverride.FilePath
		: UnrealGit::Git::FindGitBinary();
	if (GitExecutable.IsEmpty())
	{
		GitExecutable = TEXT("git");
	}
	OutSettings.GitExecutable = GitExecutable;
	OutSettings.RepositoryDiscoveryDirectory = Project && !Project->RepositoryRootOverride.Path.IsEmpty()
		? Project->RepositoryRootOverride.Path
		: FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	if (Project)
	{
		OutSettings.bPullRebase = Project->bPullRebase;
		OutSettings.bAutoPushAfterSubmit = Project->bAutoPushAfterSubmit;
		OutSettings.bEnableLfsLocks = Project->bEnableLfsLocks;
		OutSettings.LfsLocksRefreshIntervalSeconds = Project->LfsLocksRefreshIntervalSeconds;
	}

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
