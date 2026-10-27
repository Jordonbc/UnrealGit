// Copyright (C) 2026 Jordon Brooks. All Rights Reserved.

#include "UnrealGit/Git/Parsers/GitFileHistoryParser.h"

#include "UnrealGit/Private/UnrealGit/Git/Parsers/GitParsingUtils.h"

namespace UnrealGit::HistoryParsing
{
	static FString LogStatusToAction(const FString& StatusCode)
	{
		if (StatusCode.IsEmpty())
		{
			return TEXT("modified");
		}

		switch (StatusCode[0])
		{
		case TEXT('M'): return TEXT("modified");
		case TEXT('A'): return TEXT("add");
		case TEXT('D'): return TEXT("delete");
		case TEXT('R'): return TEXT("branch");
		case TEXT('C'): return TEXT("branch");
		case TEXT('T'): return TEXT("type changed");
		case TEXT('U'): return TEXT("unmerged");
		case TEXT('X'): return TEXT("unknown");
		default: return TEXT("modified");
		}
	}

	static bool ParseMetadataLine(const TArrayView<const uint8> LineBytes, FGitRevisionInfo& OutInfo)
	{
		// Fields: commit, author name, author email, author date, subject
		const TCHAR FieldSep[] = { (TCHAR)0x001F, TEXT('\0') };
		const FString Line = UnrealGit::Parsing::Utf8BytesToString(LineBytes.GetData(), LineBytes.Num());
		TArray<FString> Fields;
		Line.ParseIntoArray(Fields, FieldSep, false);
		if (Fields.Num() < 5)
		{
			return false;
		}

		OutInfo.CommitId = Fields[0];
		OutInfo.AuthorName = Fields[1];
		OutInfo.AuthorEmail = Fields[2];

		FDateTime ParsedUtc;
		if (UnrealGit::Parsing::ParseIso8601Utc(Fields[3], ParsedUtc))
		{
			OutInfo.AuthorDateUtc = ParsedUtc;
		}

		OutInfo.Subject = Fields[4];
		return true;
	}

}

bool FGitFileHistoryParser::Parse(const TArray<uint8>& StdOut, const FString& InitialRepoRelativePath, TArray<FGitFileRevision>& OutHistory, FString& OutError)
{
	OutHistory.Reset();
	OutError.Reset();

	if (StdOut.Num() == 0)
	{
		return true;
	}

	const uint8 RecordSep = 0x1E;
	const uint8 Newline = '\n';

	FString CurrentPath = InitialRepoRelativePath;

	int32 Offset = 0;
	while (Offset < StdOut.Num())
	{
		// Find the next record separator.
		while (Offset < StdOut.Num() && StdOut[Offset] != RecordSep)
		{
			++Offset;
		}

		if (Offset >= StdOut.Num())
		{
			break;
		}

		// Record begins after RS.
		const int32 RecordStart = Offset + 1;
		int32 RecordEnd = RecordStart;
		while (RecordEnd < StdOut.Num() && StdOut[RecordEnd] != RecordSep)
		{
			++RecordEnd;
		}

		const TArrayView<const uint8> RecordView = MakeArrayView(StdOut.GetData() + RecordStart, RecordEnd - RecordStart);
		Offset = RecordEnd;

		int32 LineEnd = 0;
		while (LineEnd < RecordView.Num() && RecordView[LineEnd] != Newline)
		{
			++LineEnd;
		}

		const TArrayView<const uint8> MetaLine = RecordView.Slice(0, LineEnd);
		FGitRevisionInfo Info;
		Info.Action = TEXT("modified");
		if (!UnrealGit::HistoryParsing::ParseMetadataLine(MetaLine, Info))
		{
			OutError = TEXT("Failed to parse git log metadata record.");
			return false;
		}

		FGitFileRevision FileRevision;
		FileRevision.Revision = MoveTemp(Info);
		FileRevision.RepoRelativePathAtRevision = CurrentPath;

		OutHistory.Add(MoveTemp(FileRevision));

		// Process name-status to handle renames for subsequent older commits.
		if (LineEnd >= RecordView.Num())
		{
			continue;
		}

		// Skip newline(s) separating metadata from name-status.
		int32 NameStatusOffset = LineEnd + 1;
		while (NameStatusOffset < RecordView.Num() && RecordView[NameStatusOffset] == Newline)
		{
			++NameStatusOffset;
		}
		if (NameStatusOffset >= RecordView.Num())
		{
			continue;
		}

		UnrealGit::Parsing::FNullSeparatedReader NullReader(RecordView.Slice(NameStatusOffset, RecordView.Num() - NameStatusOffset));
		TArrayView<const uint8> Token;

		while (NullReader.ReadNext(Token))
		{
			if (Token.Num() == 0)
			{
				continue;
			}

			const FString Line = UnrealGit::Parsing::Utf8BytesToString(Token.GetData(), Token.Num());
			FString StatusCode;
			FString PathA;
			if (!Line.Split(TEXT("\t"), &StatusCode, &PathA))
			{
				continue;
			}

			if (OutHistory.Num() > 0)
			{
				OutHistory.Last().Revision.Action = UnrealGit::HistoryParsing::LogStatusToAction(StatusCode);
			}

			if (StatusCode.StartsWith(TEXT("R")) || StatusCode.StartsWith(TEXT("C")))
			{
				TArrayView<const uint8> TokenB;
				if (!NullReader.ReadNext(TokenB))
				{
					continue;
				}

				const FString PathB = UnrealGit::Parsing::Utf8BytesToString(TokenB.GetData(), TokenB.Num());
				if (PathB == CurrentPath)
				{
					CurrentPath = PathA;
				}
			}
		}
	}

	const int32 NumRevisions = OutHistory.Num();
	for (int32 RevisionIndex = 0; RevisionIndex < NumRevisions; ++RevisionIndex)
	{
		OutHistory[RevisionIndex].Revision.RevisionNumber = NumRevisions - RevisionIndex;
	}

	return true;
}

static void FillSizesFromBatchCheckOutput(const FString& StdOut, TArray<FGitFileRevision>* InOutHistory, TArray<int32>* OutSizes)
{
	TArray<FString> Lines;
	StdOut.ParseIntoArray(Lines, TEXT("\n"), false);

	int32 Count = 0;
	if (InOutHistory)
	{
		Count = InOutHistory->Num();
	}
	else if (OutSizes)
	{
		int32 LineCount = Lines.Num();
		if (LineCount > 0 && Lines[LineCount - 1].IsEmpty())
		{
			--LineCount;
		}
		Count = LineCount;
	}

	if (OutSizes)
	{
		OutSizes->Reset();
	}

	for (int32 i = 0; i < Count; ++i)
	{
		int32 Size = 0;
		if (i < Lines.Num())
		{
			const FString Line = Lines[i].TrimEnd();
			TArray<FString> Tokens;
			Line.ParseIntoArrayWS(Tokens);
			if (Tokens.Num() >= 3 && Tokens[1] == TEXT("blob"))
			{
				if (Tokens[2].IsNumeric())
				{
					Size = FCString::Atoi(*Tokens[2]);
				}
			}
		}

		if (InOutHistory && i < InOutHistory->Num())
		{
			(*InOutHistory)[i].Revision.FileSize = Size;
		}
		if (OutSizes)
		{
			OutSizes->Add(Size);
		}
	}
}

void FGitFileHistoryParser::ParseCatFileBatchCheckOutput(const FString& StdOut, TArray<FGitFileRevision>& InOutRevisions)
{
	FillSizesFromBatchCheckOutput(StdOut, &InOutRevisions, nullptr);
}

void FGitFileHistoryParser::ParseCatFileBatchCheckOutput(const TArray<uint8>& StdOut, TArray<FGitFileRevision>& InOutRevisions)
{
	FillSizesFromBatchCheckOutput(UnrealGit::Parsing::Utf8BytesToString(StdOut), &InOutRevisions, nullptr);
}

void FGitFileHistoryParser::ParseCatFileBatchCheck(const FString& StdOut, TArray<FGitFileRevision>& InOutRevisions)
{
	ParseCatFileBatchCheckOutput(StdOut, InOutRevisions);
}

void FGitFileHistoryParser::ParseCatFileBatchCheck(const FString& StdOut, TArray<int32>& OutSizes)
{
	FillSizesFromBatchCheckOutput(StdOut, nullptr, &OutSizes);
}

TArray<int32> FGitFileHistoryParser::ParseCatFileBatchCheckSizes(const FString& StdOut)
{
	TArray<int32> Sizes;
	FillSizesFromBatchCheckOutput(StdOut, nullptr, &Sizes);
	return Sizes;
}

