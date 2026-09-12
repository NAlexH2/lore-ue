#include "SLoreSourceControlSettingsWidget.h"
#include "LoreSourceControlModule.h"
#include "LoreSourceControlProvider.h"
#include "LorePathUtils.h"
#include "Misc/Paths.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"

#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE "SLoreSourceControlSettings"

namespace
{
	TSharedRef<SWidget> MakeLabeledRow(const FText& Label, TSharedRef<SWidget> Field, const FText& Tooltip)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(140.0f)
				[
					SNew(STextBlock)
					.Text(Label)
					.ToolTipText(Tooltip)
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				Field
			];
	}
}

void SLoreSourceControlSettings::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
		[
			MakeLabeledRow(
				LOCTEXT("RepositoryRootLabel", "Repository folder"),
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SEditableTextBox)
					.Text(this, &SLoreSourceControlSettings::GetRepositoryRootText)
					.HintText(LOCTEXT("RepositoryRootHint", "Automatic"))
					.OnTextCommitted(this, &SLoreSourceControlSettings::OnRepositoryRootCommitted)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("BrowseRepository", "Browse..."))
					.ToolTipText(LOCTEXT("BrowseRepositoryTooltip", "Choose the folder containing .lore. Selecting .lore itself also works; its parent folder will be used."))
					.OnClicked(this, &SLoreSourceControlSettings::OnBrowseRepositoryClicked)
				],
				LOCTEXT("RepositoryRootTooltip", "The local folder containing .lore, not a .uproject file or server URL. Saved only for this project. Clear the field to restore automatic discovery, then reconnect."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("RepositoryRootHelp", "Leave empty to find .lore in this project's folder or up to four folders above it. To use another location, enter or browse to the folder containing .lore. The project must be inside it. Reconnect to apply."))
			.Font(FAppStyle::GetFontStyle("SmallFont"))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return RepositoryBrowseError; })
			.Visibility_Lambda([this]() { return RepositoryBrowseError.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			.ColorAndOpacity(FLinearColor(1.0f, 0.3f, 0.2f))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(this, &SLoreSourceControlSettings::GetDetectedRepositoryText)
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
		[
			MakeLabeledRow(
				LOCTEXT("BinaryPathLabel", "lore executable"),
				SNew(SEditableTextBox)
					.Text(this, &SLoreSourceControlSettings::GetBinaryPathText)
					.HintText(LOCTEXT("BinaryPathHint", "Leave empty to auto-detect on PATH"))
					.OnTextCommitted(this, &SLoreSourceControlSettings::OnBinaryPathCommitted),
				LOCTEXT("BinaryPathTooltip", "Full path to the `lore` executable. If empty, the plugin searches PATH and the bundled binaries."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
		[
			MakeLabeledRow(
				LOCTEXT("RepositoryUrlLabel", "Repository URL"),
				SNew(SEditableTextBox)
					.Text(this, &SLoreSourceControlSettings::GetRepositoryUrlText)
					.HintText(LOCTEXT("RepositoryUrlHint", "lore://host/repo (informational)"))
					.OnTextCommitted(this, &SLoreSourceControlSettings::OnRepositoryUrlCommitted),
				LOCTEXT("RepositoryUrlTooltip", "Remote repository URL. The working copy's .lore config is authoritative; this is informational."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)
		[
			MakeLabeledRow(
				LOCTEXT("UserNameLabel", "Identity"),
				SNew(SEditableTextBox)
					.Text(this, &SLoreSourceControlSettings::GetUserNameText)
					.HintText(LOCTEXT("UserNameHint", "Display name shown on your commits and locks"))
					.OnTextCommitted(this, &SLoreSourceControlSettings::OnUserNameCommitted),
				LOCTEXT("UserNameTooltip", "A display label attributed to your commits and locks (passed to `lore --identity`). This is NOT a login or password \u2014 server authentication, when a server requires it, is handled by the `lore` CLI itself."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 8.0f, 2.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(this, &SLoreSourceControlSettings::GetDetectedBinaryText)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 8.0f, 2.0f, 2.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Reconnect", "Reconnect"))
			.ToolTipText(LOCTEXT("ReconnectTooltip", "Re-resolve the lore binary and re-test the repository connection."))
			.OnClicked(this, &SLoreSourceControlSettings::OnReconnectClicked)
		]
	];
}

FText SLoreSourceControlSettings::GetBinaryPathText() const
{
	return FText::FromString(FLoreSourceControlModule::Get().AccessSettings().GetBinaryPath());
}

FText SLoreSourceControlSettings::GetRepositoryRootText() const
{
	return FText::FromString(FLoreSourceControlModule::Get().AccessSettings().GetRepositoryRoot());
}

void SLoreSourceControlSettings::OnRepositoryRootCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	RepositoryBrowseError = FText::GetEmpty();
	FLoreSourceControlModule& Module = FLoreSourceControlModule::Get();
	Module.AccessSettings().SetRepositoryRoot(InText.ToString().TrimStartAndEnd());
	Module.SaveSettings();
}

FReply SLoreSourceControlSettings::OnBrowseRepositoryClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		RepositoryBrowseError = LOCTEXT("RepositoryBrowserUnavailable", "Folder browsing is unavailable. Enter the repository folder path manually.");
		return FReply::Handled();
	}

	FLoreSourceControlModule& Module = FLoreSourceControlModule::Get();
	FText Error;
	FString InitialFolder = FLorePathUtils::ResolveRepositoryRoot(
		FPaths::ProjectDir(), Module.AccessSettings().GetRepositoryRoot(), Error);
	if (InitialFolder.IsEmpty())
	{
		InitialFolder = FLorePathUtils::NormalizeAbsolutePath(FPaths::ProjectDir());
	}

	FString SelectedFolder;
	const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
	if (!DesktopPlatform->OpenDirectoryDialog(ParentWindow,
		LOCTEXT("ChooseRepositoryFolder", "Choose the repository folder containing .lore").ToString(),
		InitialFolder, SelectedFolder))
	{
		return FReply::Handled(); // Cancel leaves the existing setting untouched.
	}

	FPaths::NormalizeDirectoryName(SelectedFolder);
	if (FPaths::GetCleanFilename(SelectedFolder).Equals(TEXT(".lore"), ESearchCase::IgnoreCase))
	{
		SelectedFolder = FPaths::GetPath(SelectedFolder);
	}
	const FString Root = FLorePathUtils::ResolveRepositoryRoot(FPaths::ProjectDir(), SelectedFolder, Error);
	if (Root.IsEmpty())
	{
		RepositoryBrowseError = Error; // Keep the previous setting on invalid selection.
		return FReply::Handled();
	}

	OnRepositoryRootCommitted(FText::FromString(Root), ETextCommit::Default);
	return FReply::Handled();
}

FText SLoreSourceControlSettings::GetDetectedRepositoryText() const
{
	FText Error;
	const FString Root = FLorePathUtils::ResolveRepositoryRoot(FPaths::ProjectDir(),
		FLoreSourceControlModule::Get().AccessSettings().GetRepositoryRoot(), Error);
	return Root.IsEmpty() ? Error : FText::Format(LOCTEXT("DetectedRepository", "Repository: {0}"), FText::FromString(Root));
}

void SLoreSourceControlSettings::OnBinaryPathCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	FLoreSourceControlModule& Module = FLoreSourceControlModule::Get();
	Module.AccessSettings().SetBinaryPath(InText.ToString().TrimStartAndEnd());
	Module.SaveSettings();
}

FText SLoreSourceControlSettings::GetRepositoryUrlText() const
{
	return FText::FromString(FLoreSourceControlModule::Get().AccessSettings().GetRepositoryUrl());
}

void SLoreSourceControlSettings::OnRepositoryUrlCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	FLoreSourceControlModule& Module = FLoreSourceControlModule::Get();
	Module.AccessSettings().SetRepositoryUrl(InText.ToString().TrimStartAndEnd());
	Module.SaveSettings();
}

FText SLoreSourceControlSettings::GetUserNameText() const
{
	return FText::FromString(FLoreSourceControlModule::Get().AccessSettings().GetUserName());
}

void SLoreSourceControlSettings::OnUserNameCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	FLoreSourceControlModule& Module = FLoreSourceControlModule::Get();
	Module.AccessSettings().SetUserName(InText.ToString().TrimStartAndEnd());
	Module.SaveSettings();
}

FText SLoreSourceControlSettings::GetDetectedBinaryText() const
{
	const FString Binary = FLoreSourceControlModule::Get().GetProvider().GetLoreBinaryPath();
	if (Binary.IsEmpty())
	{
		return LOCTEXT("NoBinary", "lore executable: not found");
	}
	return FText::Format(LOCTEXT("FoundBinary", "lore executable: {0}"), FText::FromString(Binary));
}

FReply SLoreSourceControlSettings::OnReconnectClicked()
{
	FLoreSourceControlModule& Module = FLoreSourceControlModule::Get();
	Module.GetProvider().Init(/*bForceConnection=*/true);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
