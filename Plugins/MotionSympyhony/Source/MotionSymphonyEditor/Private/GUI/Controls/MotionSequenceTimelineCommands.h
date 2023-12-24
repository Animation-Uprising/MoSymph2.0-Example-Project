//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"

/**
 * Defines commands for the anim sequence timeline editor
 */
class FMotionSequenceTimelineCommands : public TCommands<FMotionSequenceTimelineCommands>
{
public:
	FMotionSequenceTimelineCommands()
		: TCommands<FMotionSequenceTimelineCommands>
		(
			TEXT("MotionSequenceCurveEditor"),
			NSLOCTEXT("Contexts", "MotionSequenceTimelineEditor", "Motion Sequence Timeline Editor"),
			NAME_None,
			FAppStyle::GetAppStyleSetName()
			)
	{
	}
	
	TSharedPtr<FUICommandInfo> AddNotifyTrack;
	TSharedPtr<FUICommandInfo> InsertNotifyTrack;
	TSharedPtr<FUICommandInfo> RemoveNotifyTrack;
	TSharedPtr<FUICommandInfo> DisplaySeconds;
	TSharedPtr<FUICommandInfo> DisplayFrames;
	TSharedPtr<FUICommandInfo> DisplayPercentage;
	TSharedPtr<FUICommandInfo> DisplaySecondaryFormat;
	TSharedPtr<FUICommandInfo> SnapToFrames;
	TSharedPtr<FUICommandInfo> SnapToNotifies;
	TSharedPtr<FUICommandInfo> SnapToMontageSections;
	TSharedPtr<FUICommandInfo> SnapToCompositeSegments;

public:
	virtual void RegisterCommands() override;
};