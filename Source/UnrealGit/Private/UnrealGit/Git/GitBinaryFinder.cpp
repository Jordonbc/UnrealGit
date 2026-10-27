// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Private/UnrealGit/Git/GitBinaryFinder.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"

namespace UnrealGit::Git
{
	namespace
	{
		FString FindFirstExisting(const TArray<FString>& Candidates)
		{
			for (const FString& Candidate : Candidates)
			{
				if (IFileManager::Get().FileExists(*Candidate))
				{
					return Candidate;
				}
			}
			return FString();
		}
	}

	FString FindGitBinary()
	{
#if PLATFORM_WINDOWS
		const FString ProgramFiles = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles"));
		const FString ProgramFilesX86 = FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles(x86)"));
		const FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));

		TArray<FString> Candidates;

		// 1) Standard install directories.
		if (!ProgramFiles.IsEmpty())
		{
			Candidates.Add(FPaths::Combine(ProgramFiles, TEXT("Git"), TEXT("bin"), TEXT("git.exe")));
		}
		if (!ProgramFilesX86.IsEmpty())
		{
			Candidates.Add(FPaths::Combine(ProgramFilesX86, TEXT("Git"), TEXT("bin"), TEXT("git.exe")));
		}
		if (!LocalAppData.IsEmpty())
		{
			Candidates.Add(FPaths::Combine(LocalAppData, TEXT("Programs"), TEXT("Git"), TEXT("cmd"), TEXT("git.exe")));
		}

		// 2) SmartGit bundled git.
		if (!ProgramFilesX86.IsEmpty())
		{
			Candidates.Add(FPaths::Combine(ProgramFilesX86, TEXT("SmartGit"), TEXT("git"), TEXT("bin"), TEXT("git.exe")));
			Candidates.Add(FPaths::Combine(ProgramFilesX86, TEXT("SmartGit"), TEXT("bin"), TEXT("git.exe")));
		}

		// 3) SourceTree bundled git.
		if (!LocalAppData.IsEmpty())
		{
			Candidates.Add(FPaths::Combine(LocalAppData, TEXT("Atlassian"), TEXT("SourceTree"), TEXT("git_local"), TEXT("bin"), TEXT("git.exe")));
		}

		// 4) GitHub Desktop PortableGit.
		if (!LocalAppData.IsEmpty())
		{
			const FString SearchPath = FPaths::Combine(LocalAppData, TEXT("GitHub"), TEXT("PortableGit_*"));
			TArray<FString> PortableGitFolders;
			IFileManager::Get().FindFiles(PortableGitFolders, *SearchPath, false, true);
			if (PortableGitFolders.Num() > 0)
			{
				const FString& Folder = PortableGitFolders.Last();
				Candidates.Add(FPaths::Combine(LocalAppData, TEXT("GitHub"), Folder, TEXT("cmd"), TEXT("git.exe")));
				Candidates.Add(FPaths::Combine(LocalAppData, TEXT("GitHub"), Folder, TEXT("bin"), TEXT("git.exe")));
			}
		}

		// 5) Tower bundled git.
		if (!ProgramFilesX86.IsEmpty())
		{
			Candidates.Add(FPaths::Combine(ProgramFilesX86, TEXT("fournova"), TEXT("Tower"), TEXT("vendor"), TEXT("Git"), TEXT("bin"), TEXT("git.exe")));
		}

		const FString Found = FindFirstExisting(Candidates);
		if (!Found.IsEmpty())
		{
			return Found;
		}
#elif PLATFORM_MAC
		const FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));

		TArray<FString> Candidates;

		// 1) Official git.
		Candidates.Add(TEXT("/usr/local/git/bin/git"));
		// 2) Homebrew.
		Candidates.Add(TEXT("/usr/local/bin/git"));
		// 3) Homebrew (Apple Silicon).
		Candidates.Add(TEXT("/opt/homebrew/bin/git"));
		// 4) MacPorts.
		Candidates.Add(TEXT("/opt/local/bin/git"));
		// 5) Command Line Tools.
		Candidates.Add(TEXT("/usr/bin/git"));

		// 6) SmartGit app bundle.
		Candidates.Add(TEXT("/Applications/SmartGit.app/Contents/Resources/git/bin/git"));
		if (!Home.IsEmpty())
		{
			Candidates.Add(FPaths::Combine(Home, TEXT("Applications"), TEXT("SmartGit.app"), TEXT("Contents"), TEXT("Resources"), TEXT("git"), TEXT("bin"), TEXT("git")));
		}

		// 7) SourceTree app bundle.
		Candidates.Add(TEXT("/Applications/SourceTree.app/Contents/Resources/git_local/bin/git"));
		if (!Home.IsEmpty())
		{
			Candidates.Add(FPaths::Combine(Home, TEXT("Applications"), TEXT("SourceTree.app"), TEXT("Contents"), TEXT("Resources"), TEXT("git_local"), TEXT("bin"), TEXT("git")));
		}

		// 8) GitHub Desktop app bundle.
		Candidates.Add(TEXT("/Applications/GitHub Desktop.app/Contents/Resources/app/git/bin/git"));
		if (!Home.IsEmpty())
		{
			Candidates.Add(FPaths::Combine(Home, TEXT("Applications"), TEXT("GitHub Desktop.app"), TEXT("Contents"), TEXT("Resources"), TEXT("app"), TEXT("git"), TEXT("bin"), TEXT("git")));
		}

		// 9) Tower app bundle.
		Candidates.Add(TEXT("/Applications/Tower.app/Contents/Resources/git/bin/git"));
		if (!Home.IsEmpty())
		{
			Candidates.Add(FPaths::Combine(Home, TEXT("Applications"), TEXT("Tower.app"), TEXT("Contents"), TEXT("Resources"), TEXT("git"), TEXT("bin"), TEXT("git")));
		}

		const FString Found = FindFirstExisting(Candidates);
		if (!Found.IsEmpty())
		{
			return Found;
		}
#else
		// Linux.
		TArray<FString> Candidates;
		Candidates.Add(TEXT("/usr/bin/git"));
		Candidates.Add(TEXT("/usr/local/bin/git"));
		Candidates.Add(TEXT("/snap/bin/git"));

		const FString Found = FindFirstExisting(Candidates);
		if (!Found.IsEmpty())
		{
			return Found;
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