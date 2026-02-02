// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealGit/Git/IGitProcessRunner.h"

/**
 * Mock Git process runner used by unit tests.
 *
 * Responsibility:
 * - Captures requests and returns preprogrammed results.
 */
class FMockGitProcessRunner final : public IGitProcessRunner
{
public:
	struct FExpectation final
	{
		TArray<FString> Arguments;
		FString WorkingDirectory;
		FGitProcessResult Result;
	};

	void Enqueue(const FExpectation& Expectation)
	{
		Expectations.Enqueue(Expectation);
	}

	const TArray<FGitProcessRequest>& GetRequests() const
	{
		return Requests;
	}

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

