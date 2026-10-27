// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Git/IGitProcessRunner.h"

/**
 * @class FMockGitProcessRunner
 * Mock implementation of IGitProcessRunner used by unit tests.
 *
 * Responsibility:
 * - Records every request passed to Run for later inspection via GetRequests.
 * - Returns preprogrammed results from a FIFO queue of FExpectation entries.
 *
 * Ordered-queue semantics:
 * - Each call to Run consumes the next enqueued FExpectation in FIFO order.
 * - If no expectation remains, Run returns a failure result with ExitCode -1.
 * - Arguments and WorkingDirectory stored on an FExpectation are not matched
 *   against the incoming request; only its Result is returned.
 */
class FMockGitProcessRunner final : public IGitProcessRunner
{
public:
	/**
	 * @struct FExpectation
	 * A preprogrammed response returned by FMockGitProcessRunner::Run.
	 */
	struct FExpectation final
	{
		/** Git subcommand and arguments the test expects; currently not matched by Run. */
		TArray<FString> Arguments;
		/** Working directory the test expects; currently not matched by Run. */
		FString WorkingDirectory;
		/** Result returned verbatim when this expectation is consumed by Run. */
		FGitProcessResult Result;
	};

	/**
	 * Enqueues an expectation at the back of the FIFO queue.
	 *
	 * @param Expectation Expectation to append.
	 */
	void Enqueue(const FExpectation& Expectation)
	{
		Expectations.Enqueue(Expectation);
	}

	/**
	 * Returns the requests captured by Run, in call order.
	 *
	 * Callers can inspect each FGitProcessRequest (Arguments, RepoRoot,
	 * WorkingDirectory, StdIn, Timeout) to assert what was submitted.
	 *
	 * @return Captured request list.
	 */
	const TArray<FGitProcessRequest>& GetRequests() const
	{
		return Requests;
	}

	/**
	 * Consumes the next queued expectation and returns its Result.
	 *
	 * Records Request in GetRequests before consuming. If the queue is empty,
	 * returns a failure result with ExitCode -1.
	 *
	 * @param Request Git request to record and (conceptually) execute.
	 * @return Preprogrammed result, or a failure result when no expectation remains.
	 */
	virtual FGitProcessResult Run(const FGitProcessRequest& Request) override
	{
		Requests.Add(Request);

		FExpectation Expected;
		if (!Expectations.Dequeue(Expected))
		{
			FGitProcessResult Fail;
			Fail.ExitCode = -1;
			return Fail;
		}

		return Expected.Result;
	}

private:
	TQueue<FExpectation> Expectations;
	TArray<FGitProcessRequest> Requests;
};
