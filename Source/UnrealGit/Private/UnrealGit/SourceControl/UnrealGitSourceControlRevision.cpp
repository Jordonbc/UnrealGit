// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealGit/SourceControl/UnrealGitSourceControlRevision.h"

#include "Async/Async.h"
#include "Misc/Parse.h"
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
		// Synchronous request on the game thread: run git on a worker thread and block
		// until materialization completes, since ProcessRunner cannot run on the game thread.
		{
			FScopeLock Scope(&Lock);
			if (CachedMaterializedFilename.IsSet())
			{
				InOutFilename = CachedMaterializedFilename.GetValue();
				return true;
			}
		}

		const TSharedRef<IGitProcessRunner, ESPMode::ThreadSafe> Runner = ProcessRunner;
		const TSharedRef<FGitRevisionMaterializer, ESPMode::ThreadSafe> Mat = Materializer;
		const FString Root = RepoRoot;
		const FString Commit = RevisionInfo.CommitId;
		const FString Path = RepoRelativePath;

		FString Filename;
		FString MatError;
		TFuture<bool> Future = Async(EAsyncExecution::ThreadPool, [Runner, Mat, Root, Commit, Path, &Filename, &MatError]()
		{
			return Mat->Materialize(Runner, Root, Commit, Path, Filename, MatError);
		});

		const bool bOk = Future.Get();
		if (bOk)
		{
			FScopeLock Scope(&Lock);
			CachedMaterializedFilename = Filename;
			InOutFilename = Filename;
		}
		return bOk;
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
	return RevisionInfo.RevisionNumber;
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
	static const FString DefaultAction(TEXT("edit"));
	return RevisionInfo.Action.IsEmpty() ? DefaultAction : RevisionInfo.Action;
}

int32 FUnrealGitSourceControlRevision::GetCheckInIdentifier() const
{
	// Git has no numeric changeset id; use a stable prefix of the commit hash.
	return RevisionInfo.CommitId.IsEmpty() ? 0 : static_cast<int32>(FParse::HexNumber(*RevisionInfo.CommitId.Left(8)));
}

int32 FUnrealGitSourceControlRevision::GetFileSize() const
{
	return RevisionInfo.FileSize;
}

const FString& FUnrealGitSourceControlRevision::GetClientSpec() const
{
	static const FString EmptyClientSpec;
	return EmptyClientSpec;
}

TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> FUnrealGitSourceControlRevision::GetBranchSource() const
{
	return BranchSource;
}

void FUnrealGitSourceControlRevision::SetBranchSource(TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> InBranchSource)
{
	BranchSource = MoveTemp(InBranchSource);
}

bool FUnrealGitSourceControlRevision::GetAnnotated(FString& /*InOutFilename*/) const
{
	return false;
}

bool FUnrealGitSourceControlRevision::GetAnnotated(TArray<FAnnotationLine>& /*OutLines*/) const
{
	return false;
}
