// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/Git/GitBinaryFinder.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"

namespace UnrealGit::Git
{
	FString FindGitBinary()
	{
#if PLATFORM_WINDOWS
		// 1) Standard install directories.
		{
			const FString ProgramFiles = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles"));
			const FString Candidate = FPaths::Combine(ProgramFiles, TEXT("Git"), TEXT("bin"), TEXT("git.exe"));
			if (!ProgramFiles.IsEmpty() && IFileManager::Get().FileExists(*Candidate))
			{
				return Candidate;
			}
		}
		{
			const FString ProgramFilesX86 = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles(x86)"));
			const FString Candidate = FPaths::Combine(ProgramFilesX86, TEXT("Git"), TEXT("bin"), TEXT("git.exe"));
			if (!ProgramFilesX86.IsEmpty() && IFileManager::Get().FileExists(*Candidate))
			{
				return Candidate;
			}
		}
		{
			const FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
			const FString Candidate = FPaths::Combine(LocalAppData, TEXT("Programs"), TEXT("Git"), TEXT("cmd"), TEXT("git.exe"));
			if (!LocalAppData.IsEmpty() && IFileManager::Get().FileExists(*Candidate))
			{
				return Candidate;
			}
		}

		// 2) SmartGit bundled git.
		{
			const FString ProgramFilesX86 = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles(x86)"));
			if (!ProgramFilesX86.IsEmpty())
			{
				FString Candidate = FPaths::Combine(ProgramFilesX86, TEXT("SmartGit"), TEXT("git"), TEXT("bin"), TEXT("git.exe"));
				if (IFileManager::Get().FileExists(*Candidate))
				{
					return Candidate;
				}
				Candidate = FPaths::Combine(ProgramFilesX86, TEXT("SmartGit"), TEXT("bin"), TEXT("git.exe"));
				if (IFileManager::Get().FileExists(*Candidate))
				{
					return Candidate;
				}
			}
		}

		// 3) SourceTree bundled git.
		{
			const FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
			if (!LocalAppData.IsEmpty())
			{
				const FString Candidate = FPaths::Combine(LocalAppData, TEXT("Atlassian"), TEXT("SourceTree"), TEXT("git_local"), TEXT("bin"), TEXT("git.exe"));
				if (IFileManager::Get().FileExists(*Candidate))
				{
					return Candidate;
				}
			}
		}

		// 4) GitHub Desktop PortableGit.
		{
			const FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
			if (!LocalAppData.IsEmpty())
			{
				const FString SearchPath = FPaths::Combine(LocalAppData, TEXT("GitHub"), TEXT("PortableGit_*"));
				TArray<FString> PortableGitFolders;
				IFileManager::Get().FindFiles(PortableGitFolders, *SearchPath, false, true);
				if (PortableGitFolders.Num() > 0)
				{
					const FString& Folder = PortableGitFolders.Last();
					FString Candidate = FPaths::Combine(LocalAppData, TEXT("GitHub"), Folder, TEXT("cmd"), TEXT("git.exe"));
					if (IFileManager::Get().FileExists(*Candidate))
					{
						return Candidate;
					}
					Candidate = FPaths::Combine(LocalAppData, TEXT("GitHub"), Folder, TEXT("bin"), TEXT("git.exe"));
					if (IFileManager::Get().FileExists(*Candidate))
					{
						return Candidate;
					}
				}
			}
		}

		// 5) Tower bundled git.
		{
			const FString ProgramFilesX86 = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles(x86)"));
			if (!ProgramFilesX86.IsEmpty())
			{
				const FString Candidate = FPaths::Combine(ProgramFilesX86, TEXT("fournova"), TEXT("Tower"), TEXT("vendor"), TEXT("Git"), TEXT("bin"), TEXT("git.exe"));
				if (IFileManager::Get().FileExists(*Candidate))
				{
					return Candidate;
				}
			}
		}
#elif PLATFORM_MAC
		// 1) Official git.
		if (IFileManager::Get().FileExists(TEXT("/usr/local/git/bin/git")))
		{
			return TEXT("/usr/local/git/bin/git");
		}
		// 2) Homebrew.
		if (IFileManager::Get().FileExists(TEXT("/usr/local/bin/git")))
		{
			return TEXT("/usr/local/bin/git");
		}
		// 3) Homebrew (Apple Silicon).
		if (IFileManager::Get().FileExists(TEXT("/opt/homebrew/bin/git")))
		{
			return TEXT("/opt/homebrew/bin/git");
		}
		// 4) MacPorts.
		if (IFileManager::Get().FileExists(TEXT("/opt/local/bin/git")))
		{
			return TEXT("/opt/local/bin/git");
		}
		// 5) Command Line Tools.
		if (IFileManager::Get().FileExists(TEXT("/usr/bin/git")))
		{
			return TEXT("/usr/bin/git");
		}

		const FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));

		// 6) SmartGit app bundle.
		{
			const FString Candidate = TEXT("/Applications/SmartGit.app/Contents/Resources/git/bin/git");
			if (IFileManager::Get().FileExists(*Candidate))
			{
				return Candidate;
			}
			if (!Home.IsEmpty())
			{
				const FString UserCandidate = FPaths::Combine(Home, TEXT("Applications"), TEXT("SmartGit.app"), TEXT("Contents"), TEXT("Resources"), TEXT("git"), TEXT("bin"), TEXT("git"));
				if (IFileManager::Get().FileExists(*UserCandidate))
				{
					return UserCandidate;
				}
			}
		}

		// 7) SourceTree app bundle.
		{
			const FString Candidate = TEXT("/Applications/SourceTree.app/Contents/Resources/git_local/bin/git");
			if (IFileManager::Get().FileExists(*Candidate))
			{
				return Candidate;
			}
			if (!Home.IsEmpty())
			{
				const FString UserCandidate = FPaths::Combine(Home, TEXT("Applications"), TEXT("SourceTree.app"), TEXT("Contents"), TEXT("Resources"), TEXT("git_local"), TEXT("bin"), TEXT("git"));
				if (IFileManager::Get().FileExists(*UserCandidate))
				{
					return UserCandidate;
				}
			}
		}

		// 8) GitHub Desktop app bundle.
		{
			const FString Candidate = TEXT("/Applications/GitHub Desktop.app/Contents/Resources/app/git/bin/git");
			if (IFileManager::Get().FileExists(*Candidate))
			{
				return Candidate;
			}
			if (!Home.IsEmpty())
			{
				const FString UserCandidate = FPaths::Combine(Home, TEXT("Applications"), TEXT("GitHub Desktop.app"), TEXT("Contents"), TEXT("Resources"), TEXT("app"), TEXT("git"), TEXT("bin"), TEXT("git"));
				if (IFileManager::Get().FileExists(*UserCandidate))
				{
					return UserCandidate;
				}
			}
		}

		// 9) Tower app bundle.
		{
			const FString Candidate = TEXT("/Applications/Tower.app/Contents/Resources/git/bin/git");
			if (IFileManager::Get().FileExists(*Candidate))
			{
				return Candidate;
			}
			if (!Home.IsEmpty())
			{
				const FString UserCandidate = FPaths::Combine(Home, TEXT("Applications"), TEXT("Tower.app"), TEXT("Contents"), TEXT("Resources"), TEXT("git"), TEXT("bin"), TEXT("git"));
				if (IFileManager::Get().FileExists(*UserCandidate))
				{
					return UserCandidate;
				}
			}
		}
#else
		// Linux.
		if (IFileManager::Get().FileExists(TEXT("/usr/bin/git")))
		{
			return TEXT("/usr/bin/git");
		}
		if (IFileManager::Get().FileExists(TEXT("/usr/local/bin/git")))
		{
			return TEXT("/usr/local/bin/git");
		}
		if (IFileManager::Get().FileExists(TEXT("/snap/bin/git")))
		{
			return TEXT("/snap/bin/git");
		}
#endif

		// Fall back to a bare name so PATH lookup still applies.
		return TEXT("git");
	}

	bool IsValidGitBinary(const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return false;
		}

		// Bare names (no path separators) are assumed to be resolved via PATH.
		if (!Path.Contains(TEXT("/")) && !Path.Contains(TEXT("\\")))
		{
			return true;
		}

		return IFileManager::Get().FileExists(*Path);
	}
}