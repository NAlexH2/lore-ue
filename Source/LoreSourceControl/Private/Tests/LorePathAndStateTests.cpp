#if WITH_DEV_AUTOMATION_TESTS

#include "LorePathUtils.h"
#include "LoreSourceControlState.h"
#include "LoreSourceControlRevision.h"
#include "LoreSourceControlUtils.h"
#include "LoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLoreRepositoryRootTest,
	"Lore.SourceControl.PathUtils.RepositoryRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLoreRepositoryRootTest::RunTest(const FString& Parameters)
{
	const FString Base = FLorePathUtils::NormalizeAbsolutePath(FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("LoreRootTests"), FGuid::NewGuid().ToString()));
	IFileManager& Files = IFileManager::Get();
	ON_SCOPE_EXIT { Files.DeleteDirectory(*Base, false, true); };
	const FString Root = FPaths::Combine(Base, TEXT("Repo"));
	const FString Deep = FPaths::Combine(Root, TEXT("One/Two/Three/Four/Five"));
	TestTrue(TEXT("Create fixture"), Files.MakeDirectory(*Deep, true));
	TestTrue(TEXT("Create metadata"), Files.MakeDirectory(*FPaths::Combine(Root, TEXT(".lore")), true));
	FText Error;
	auto Resolve = [&Error](const FString& Project, const FString& Override = FString())
	{
		return FLorePathUtils::MakeCacheKey(FLorePathUtils::ResolveRepositoryRoot(Project, Override, Error));
	};
	const FString RootKey = FLorePathUtils::MakeCacheKey(Root);
	TestEqual(TEXT("Project is repository root"), Resolve(Root), RootKey);
	TestEqual(TEXT("One parent"), Resolve(FPaths::Combine(Root, TEXT("One"))), RootKey);
	TestEqual(TEXT("Fourth parent included"), Resolve(FPaths::GetPath(Deep)), RootKey);
	TestTrue(TEXT("Fifth parent excluded"), Resolve(Deep).IsEmpty());
	TestFalse(TEXT("Missing repository explains selection"), Error.IsEmpty());
	TestEqual(TEXT("Explicit root bypasses limit"), Resolve(Deep, Root), RootKey);
	TestTrue(TEXT("Success clears previous error"), Error.IsEmpty());
	TestEqual(TEXT("Normalize parent traversal"), Resolve(Deep, FPaths::Combine(Root, TEXT("One/.."))), RootKey);
	TestTrue(TEXT("Metadata directory itself rejected"), Resolve(Deep, FPaths::Combine(Root, TEXT(".lore"))).IsEmpty());
	TestTrue(TEXT("Invalid override does not fall back"), Resolve(Root, Base).IsEmpty());
	TestTrue(TEXT("Relative override rejected"), Resolve(Root, TEXT("Repo")).IsEmpty());
	TestTrue(TEXT("Sibling sharing prefix rejected"), Resolve(Root + TEXT("Other"), Root).IsEmpty());
	TestTrue(TEXT("Project above selected repository rejected"), Resolve(Base, Root).IsEmpty());
	const FString Nested = FPaths::Combine(Root, TEXT("One/Two"));
	TestTrue(TEXT("Create nested metadata"), Files.MakeDirectory(*FPaths::Combine(Nested, TEXT(".lore")), true));
	TestEqual(TEXT("Nearest nested repository wins"), Resolve(Deep), FLorePathUtils::MakeCacheKey(Nested));
	TestEqual(TEXT("Override wins over automatic discovery"), Resolve(Deep, Root), RootKey);
	const FString Asset = FPaths::Combine(Root, TEXT("One/Content/Asset.uasset"));
	TestEqual(TEXT("Nested project file uses repository-relative path"),
		FLorePathUtils::ToWorkspaceRelativePath(Asset, Root), FString(TEXT("One/Content/Asset.uasset")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLorePathUtilsTest,
	"Lore.SourceControl.PathUtils.WorkspaceRelative",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLorePathUtilsTest::RunTest(const FString& Parameters)
{
	const FString Root = FLorePathUtils::NormalizeAbsolutePath(FPaths::ProjectDir());
	const FString File = FLorePathUtils::NormalizeAbsolutePath(FPaths::Combine(Root, TEXT("Content/Test/Asset.uasset")));

	TestTrue(TEXT("Path is under workspace"), FLorePathUtils::IsUnderWorkspace(File, Root));

	const FString Relative = FLorePathUtils::ToWorkspaceRelativePath(File, Root);
	TestFalse(TEXT("Relative path is not empty"), Relative.IsEmpty());
	TestTrue(TEXT("Relative path is relative"), FPaths::IsRelative(Relative));

	const FString RoundTrip = FLorePathUtils::ToAbsolutePath(Relative, Root);
	TestEqual(TEXT("Relative to absolute round-trips"), FLorePathUtils::MakeCacheKey(RoundTrip), FLorePathUtils::MakeCacheKey(File));

	const FString EnginePath = FLorePathUtils::NormalizeAbsolutePath(TEXT("C:/Program Files/Epic Games/UE_5.7/Engine/Config/DefaultEngine.ini"));
	TestFalse(TEXT("Engine path is outside project workspace"), FLorePathUtils::IsUnderWorkspace(EnginePath, Root));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLoreStripAnsiTest,
	"Lore.SourceControl.Parsers.StripAnsi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLoreStripAnsiTest::RunTest(const FString& Parameters)
{
	const FString Colored = FString::Printf(TEXT("%c[1;32mChanges staged for commit:%c[0m"), 0x1B, 0x1B);
	TestEqual(TEXT("ANSI codes are stripped"), LoreSourceControlUtils::StripAnsi(Colored), FString(TEXT("Changes staged for commit:")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLoreStatusParseTest,
	"Lore.SourceControl.Parsers.Status",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLoreStatusParseTest::RunTest(const FString& Parameters)
{
	const FString Root = FLorePathUtils::NormalizeAbsolutePath(FPaths::ProjectDir());

	TArray<FString> Lines;
	Lines.Add(TEXT("Repository deadbeefcafe"));
	Lines.Add(TEXT("On branch main revision 4 -> abc123def456"));
	Lines.Add(TEXT("Local branch is behind remote"));
	Lines.Add(TEXT("Changes staged for commit:"));
	Lines.Add(TEXT("A Content/New.uasset"));
	Lines.Add(TEXT("M Content/Changed.uasset"));
	Lines.Add(TEXT("Untracked files:"));
	Lines.Add(TEXT("A Content/Untracked.txt"));

	FLoreRepoStatus Repo;
	TArray<FLoreSourceControlState> States;
	LoreSourceControlUtils::ParseStatusResults(Lines, Root, Repo, States);

	TestEqual(TEXT("Branch parsed"), Repo.BranchName, FString(TEXT("main")));
	TestEqual(TEXT("Local revision number parsed"), Repo.LocalRevisionNumber, 4);
	TestEqual(TEXT("Local revision hash parsed"), Repo.LocalRevision, FString(TEXT("abc123def456")));
	TestTrue(TEXT("Remote ahead detected"), Repo.bRemoteAhead);
	TestEqual(TEXT("Three files parsed"), States.Num(), 3);

	auto FindState = [&States, &Root](const TCHAR* Rel) -> const FLoreSourceControlState*
	{
		const FString Abs = FLorePathUtils::MakeCacheKey(FLorePathUtils::ToAbsolutePath(Rel, Root));
		for (const FLoreSourceControlState& S : States)
		{
			if (FLorePathUtils::MakeCacheKey(S.LocalFilename) == Abs)
			{
				return &S;
			}
		}
		return nullptr;
	};

	const FLoreSourceControlState* Added = FindState(TEXT("Content/New.uasset"));
	const FLoreSourceControlState* Modified = FindState(TEXT("Content/Changed.uasset"));
	const FLoreSourceControlState* Untracked = FindState(TEXT("Content/Untracked.txt"));

	TestTrue(TEXT("Added state present"), Added != nullptr && Added->WorkingState == ELoreWorkingState::Added);
	TestTrue(TEXT("Modified state present"), Modified != nullptr && Modified->WorkingState == ELoreWorkingState::Modified);
	TestTrue(TEXT("Untracked state present"), Untracked != nullptr && Untracked->WorkingState == ELoreWorkingState::NotControlled);
	if (Modified != nullptr)
	{
		TestTrue(TEXT("Out-of-date propagated to file state"), Modified->bNewerVersionOnServer);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLoreLockParseTest,
	"Lore.SourceControl.Parsers.LockQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLoreLockParseTest::RunTest(const FString& Parameters)
{
	TArray<FString> Lines;
	Lines.Add(TEXT("Locks found:"));
	Lines.Add(TEXT("Content/Locked.uasset by alice on branch main"));

	TArray<FLoreLockInfo> Locks;
	LoreSourceControlUtils::ParseLockQueryResults(Lines, Locks);

	TestEqual(TEXT("One lock parsed"), Locks.Num(), 1);
	if (Locks.Num() == 1)
	{
		TestEqual(TEXT("Lock path"), Locks[0].Path, FString(TEXT("Content/Locked.uasset")));
		TestEqual(TEXT("Lock owner"), Locks[0].Owner, FString(TEXT("alice")));
		TestEqual(TEXT("Lock branch"), Locks[0].Branch, FString(TEXT("main")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLoreHistoryParseTest,
	"Lore.SourceControl.Parsers.History",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLoreHistoryParseTest::RunTest(const FString& Parameters)
{
	TArray<FString> Lines;
	Lines.Add(TEXT("M"));
	Lines.Add(TEXT("Revision  : 5"));
	Lines.Add(TEXT("Signature : abc123"));
	Lines.Add(TEXT("Address   : 00ff"));
	Lines.Add(TEXT("Committer : bob"));
	Lines.Add(TEXT("Branch    : main"));
	Lines.Add(TEXT("    Did a thing"));
	Lines.Add(TEXT(""));
	Lines.Add(TEXT("A Content/New.uasset"));
	Lines.Add(TEXT("Revision  : 4"));
	Lines.Add(TEXT("Signature : def456"));
	Lines.Add(TEXT("Committer : alice"));
	Lines.Add(TEXT("    Initial add"));

	TArray<TSharedRef<FLoreSourceControlRevision, ESPMode::ThreadSafe>> History;
	LoreSourceControlUtils::ParseHistoryResults(Lines, TEXT("C:/proj/Content/New.uasset"), TEXT("Content/New.uasset"), History);

	TestEqual(TEXT("Two revisions parsed"), History.Num(), 2);
	if (History.Num() == 2)
	{
		TestEqual(TEXT("Newest revision number"), History[0]->GetRevisionNumber(), 5);
		TestEqual(TEXT("Newest signature"), History[0]->GetRevision(), FString(TEXT("abc123")));
		TestEqual(TEXT("Newest committer"), History[0]->GetUserName(), FString(TEXT("bob")));
		TestEqual(TEXT("Newest message"), History[0]->GetDescription(), FString(TEXT("Did a thing")));
		TestEqual(TEXT("Older revision number"), History[1]->GetRevisionNumber(), 4);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLoreStateTransitionTest,
	"Lore.SourceControl.State.Transitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLoreStateTransitionTest::RunTest(const FString& Parameters)
{
	const FString File = FLorePathUtils::NormalizeAbsolutePath(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("LoreStateMapping.uasset")));

	// Modified + locked by me + at head -> checked out and submittable.
	// Unreal filters submitted existing files using IsCheckedOut after refreshing
	// status. An unlocked edit must survive that filter without claiming a lock.
	{
		FLoreSourceControlState State(File);
		State.WorkingState = ELoreWorkingState::Modified;
		TestTrue(TEXT("Unlocked modified file can check in"), State.CanCheckIn());
		TestTrue(TEXT("Unlocked edit survives Unreal submit filter"),
			State.IsCheckedOut() || State.IsAdded() || State.IsDeleted());
		TestTrue(TEXT("Unlocked edit is not reverted as unchanged"), State.IsModified());
		TestTrue(TEXT("Explicit lock acquisition remains available"), State.CanCheckout());
		State.WorkingState = ELoreWorkingState::Clean;
		TestFalse(TEXT("Clean unlocked file is not checked out"), State.IsCheckedOut());
		State.WorkingState = ELoreWorkingState::Modified;
		State.LockState = ELoreLockState::LockedByOther;
		TestFalse(TEXT("Foreign lock is not our checkout"), State.IsCheckedOut());
		TestFalse(TEXT("Foreign lock still blocks submit"), State.CanCheckIn());
	}

	{
		FLoreSourceControlState State(File);
		State.WorkingState = ELoreWorkingState::Modified;
		State.LockState = ELoreLockState::LockedByMe;
		State.bNewerVersionOnServer = false;
		TestTrue(TEXT("Modified is source controlled"), State.IsSourceControlled());
		TestTrue(TEXT("Locked-by-me is checked out"), State.IsCheckedOut());
		TestTrue(TEXT("Modified+at-head can check in"), State.CanCheckIn());
		TestTrue(TEXT("Modified is modified"), State.IsModified());
	}

	// Locked by another user -> cannot checkout, reports owner, blocks submit.
	{
		FLoreSourceControlState State(File);
		State.WorkingState = ELoreWorkingState::Modified;
		State.LockState = ELoreLockState::LockedByOther;
		State.LockUser = TEXT("OtherUser");
		FString Owner;
		TestTrue(TEXT("Reports checked-out-by-other"), State.IsCheckedOutOther(&Owner));
		TestEqual(TEXT("Lock owner reported"), Owner, FString(TEXT("OtherUser")));
		TestFalse(TEXT("Cannot checkout when locked by other"), State.CanCheckout());
		TestFalse(TEXT("Cannot check in when locked by other"), State.CanCheckIn());
	}

	// Out of date -> cannot submit even when locked by me.
	{
		FLoreSourceControlState State(File);
		State.WorkingState = ELoreWorkingState::Modified;
		State.LockState = ELoreLockState::LockedByMe;
		State.bNewerVersionOnServer = true;
		TestFalse(TEXT("Out-of-date file is not current"), State.IsCurrent());
		TestFalse(TEXT("Out-of-date file cannot check in"), State.CanCheckIn());
	}

	// Untracked -> can add, not source controlled.
	{
		FLoreSourceControlState State(File);
		State.WorkingState = ELoreWorkingState::NotControlled;
		TestTrue(TEXT("Untracked can add"), State.CanAdd());
		TestFalse(TEXT("Untracked is not source controlled"), State.IsSourceControlled());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLoreCliSmokeTest,
	"Lore.SourceControl.Cli.Available",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLoreCliSmokeTest::RunTest(const FString& Parameters)
{
	// This is a soft gate: when the `lore` binary is not installed the test
	// passes (the provider degrades gracefully). When it is present, it must
	// report a version.
	const FString Binary = LoreSourceControlUtils::FindLoreBinaryPath(FString());
	if (Binary.IsEmpty())
	{
		AddInfo(TEXT("`lore` executable not found on PATH; skipping live CLI smoke check."));
		return true;
	}

	FString Version;
	const bool bAvailable = LoreSourceControlUtils::CheckLoreAvailable(Binary, Version);
	TestTrue(TEXT("lore --version succeeds when binary present"), bAvailable);
	TestFalse(TEXT("lore version string is non-empty"), Version.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
