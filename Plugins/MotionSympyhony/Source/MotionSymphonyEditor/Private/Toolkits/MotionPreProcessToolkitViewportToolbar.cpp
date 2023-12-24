//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MotionPreProcessToolkitViewportToolbar.h"
#include "SEditorViewport.h"
#include "MotionPreProcessorToolkitCommands.h"

#define LOCTEXT_NAMESPACE "SMotionPreProcessToolkitViewportToolbar"

void SMotionPreProcessToolkitViewportToolbar::Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider)
{
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InInfoProvider);
}


TSharedRef<SWidget> SMotionPreProcessToolkitViewportToolbar::GenerateShowMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();

	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{
		ShowMenuBuilder.AddMenuEntry(FMotionPreProcessToolkitCommands::Get().SetShowMirrored);
		ShowMenuBuilder.AddMenuSeparator();
		ShowMenuBuilder.AddMenuEntry(FMotionPreProcessToolkitCommands::Get().SetShowPivot);
		ShowMenuBuilder.AddMenuEntry(FMotionPreProcessToolkitCommands::Get().SetShowPose);
		ShowMenuBuilder.AddMenuEntry(FMotionPreProcessToolkitCommands::Get().SetShowTrajectory);
		ShowMenuBuilder.AddMenuSeparator();
		ShowMenuBuilder.AddMenuEntry(FMotionPreProcessToolkitCommands::Get().SetShowGrid);
		ShowMenuBuilder.AddMenuEntry(FMotionPreProcessToolkitCommands::Get().SetShowBounds);
		ShowMenuBuilder.AddMenuSeparator();
		ShowMenuBuilder.AddMenuEntry(FMotionPreProcessToolkitCommands::Get().SetShowCollision);
	}
	
	return ShowMenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE

