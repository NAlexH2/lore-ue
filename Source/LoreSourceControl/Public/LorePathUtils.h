#pragma once

#include "CoreMinimal.h"

/** Path normalization helpers. Cache keys are normalized absolute filesystem paths. */
class FLorePathUtils
{
public:
	/** Project directory plus at most four parents; an explicit absolute root bypasses the depth limit. */
	static FString ResolveRepositoryRoot(const FString& ProjectDirectory, const FString& Override, FText& OutError);
	static FString NormalizeAbsolutePath(const FString& InPath);
	static FString NormalizeSourceControlPath(const FString& InPath, const FString& WorkspaceRoot);
	static FString ResolveSourceControlPath(const FString& InPath, const FString& WorkspaceRoot);
	static FString ToWorkspaceRelativePath(const FString& AbsolutePath, const FString& WorkspaceRoot);
	static FString ToAbsolutePath(const FString& WorkspaceRelativePath, const FString& WorkspaceRoot);
	static TArray<FString> NormalizeFiles(const TArray<FString>& InFiles);
	static TArray<FString> NormalizeFiles(const TArray<FString>& InFiles, const FString& WorkspaceRoot);
	static bool IsUnderWorkspace(const FString& AbsolutePath, const FString& WorkspaceRoot);
	static FString MakeCacheKey(const FString& InPath);
};
