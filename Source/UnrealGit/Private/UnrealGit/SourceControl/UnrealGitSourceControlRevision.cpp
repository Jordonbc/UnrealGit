// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/SourceControl/UnrealGitSourceControlRevision.h"

#include "Async/Async.h"
#include "UnrealGit/Git/GitRevisionMaterializer.h"
#include "UnrealGit/Git/IGitProcessRunner.h"

FUnrealGitSourceControlRevision::FUnrealGitSourceControlRevision(
	FString InAbsoluteFilename,
	FString InRepoRelativePath,
	FGitRevisionInfo InRevisionInfo,
	FString InRepoRoot,
	TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe> InProcessRunner,
	TSharedRef<FGitRevisionMaterializer, ESPMode::ThreadSafe> InMaterializer)
	: AbsoluteFilename(MoveTemp(InAbsoluteFilename))
	, RepoRelativePath(MoveTemp(InRepoRelativePath))
	, RevisionInfo(MoveTemp(InRevisionInfo))
	, RepoRoot(MoveTemp(InRepoRoot))
	, ProcessRunner(MoveTemp(InProcessRunner))
	, Materializer(MoveTemp(InMaterializer))
{
	CachedAction = TEXT("edit");
}

bool FUnrealGitSourceControlRevision::TryCompleteMaterialization(FString& InOutFilename) const
{
	{
		FScopeLock Scope(&Lock);
		if (CachedMaterializedFilename.IsSet())
		{
			InOutFilename = CachedMaterializedFilename.GetValue();
			return true;
		}
	}

	TSharedPtr<FMaterializationTask, ESPMode::ThreadSafe> LocalTask;
	{
		FScopeLock Scope(&Lock);
		LocalTask = Task;
	}

	if (!LocalTask.IsValid())
	{
		return false;
	}

	if (!LocalTask->Future.IsReady())
	{
		return false;
	}

	const bool bOk = LocalTask->Future.Get();
	{
		FScopeLock Scope(&Lock);
		Task.Reset();
		if (bOk)
		{
			CachedMaterializedFilename = LocalTask->MaterializedFilename;
			InOutFilename = LocalTask->MaterializedFilename;
		}
	}

	return bOk;
}

bool FUnrealGitSourceControlRevision::StartMaterializationAsync() const
{
	FScopeLock Scope(&Lock);
	if (CachedMaterializedFilename.IsSet())
	{
		return true;
	}
	if (Task.IsValid())
	{
		return true;
	}

	Task = MakeShared<FMaterializationTask, ESPMode::ThreadSafe>();
	Task->Future = Async(EAsyncExecution::ThreadPool, [LocalTask = Task, Runner = ProcessRunner, LocalMaterializer = Materializer, Root = RepoRoot, Commit = RevisionInfo.CommitId, Path = RepoRelativePath]()
	{
		FString Filename;
		FString Error;
		const bool bOk = LocalMaterializer->Materialize(Runner, Root, Commit, Path, Filename, Error);
		LocalTask->MaterializedFilename = MoveTemp(Filename);
		LocalTask->Error = MoveTemp(Error);
		return bOk;
	});

	return true;
}

bool FUnrealGitSourceControlRevision::MaterializeSync(FString& InOutFilename) const
{
	{
		FScopeLock Scope(&Lock);
		if (CachedMaterializedFilename.IsSet())
		{
			InOutFilename = CachedMaterializedFilename.GetValue();
			return true;
		}
	}

	FString Filename;
	FString Error;
	const bool bOk = Materializer->Materialize(ProcessRunner, RepoRoot, RevisionInfo.CommitId, RepoRelativePath, Filename, Error);
	if (bOk)
	{
		FScopeLock Scope(&Lock);
		CachedMaterializedFilename = Filename;
		InOutFilename = MoveTemp(Filename);
	}
	return bOk;
}

bool FUnrealGitSourceControlRevision::Get(FString& InOutFilename, EConcurrency::Type InConcurrency) const
{
	if (TryCompleteMaterialization(InOutFilename))
	{
		return true;
	}

	if (InConcurrency == EConcurrency::Asynchronous)
	{
		StartMaterializationAsync();
		return true;
	}

	if (IsInGameThread())
	{
		return false;
	}

	return MaterializeSync(InOutFilename);
}

const FString& FUnrealGitSourceControlRevision::GetFilename() const
{
	return AbsoluteFilename;
}

const FString& FUnrealGitSourceControlRevision::GetRevision() const
{
	return RevisionInfo.CommitId;
}

int32 FUnrealGitSourceControlRevision::GetRevisionNumber() const
{
	return 0;
}

const FString& FUnrealGitSourceControlRevision::GetDescription() const
{
	static const FString Empty;
	return RevisionInfo.Subject.IsEmpty() ? Empty : RevisionInfo.Subject;
}

const FString& FUnrealGitSourceControlRevision::GetUserName() const
{
	return RevisionInfo.AuthorName;
}

const FDateTime& FUnrealGitSourceControlRevision::GetDate() const
{
	return RevisionInfo.AuthorDateUtc;
}

const FString& FUnrealGitSourceControlRevision::GetAction() const
{
	return CachedAction;
}

int32 FUnrealGitSourceControlRevision::GetCheckInIdentifier() const
{
	return 0;
}

int32 FUnrealGitSourceControlRevision::GetFileSize() const
{
	return 0;
}

const FString& FUnrealGitSourceControlRevision::GetClientSpec() const
{
	return CachedClientSpec;
}

TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> FUnrealGitSourceControlRevision::GetBranchSource() const
{
	return TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe>();
}

bool FUnrealGitSourceControlRevision::GetAnnotated(FString& /*InOutFilename*/) const
{
	return false;
}

bool FUnrealGitSourceControlRevision::GetAnnotated(TArray<FAnnotationLine>& /*OutLines*/) const
{
	return false;
}
