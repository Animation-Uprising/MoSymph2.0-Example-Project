//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MotionSequenceTimelineCommands.h"

#define LOCTEXT_NAMESPACE "MotionSequenceTimelineCommands"

void FMotionSequenceTimelineCommands::RegisterCommands()
{
	UI_COMMAND(AddNotifyTrack, "Add Tag Track", "Add a new tag track", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(InsertNotifyTrack, "Insert Tag Track", "Insert a new tag track above here", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RemoveNotifyTrack, "Remove Tag Track", "Remove this tag track", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DisplaySeconds, "Seconds", "Display the time in seconds", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(DisplayFrames, "Frames", "Display the time in frames", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(DisplayPercentage, "Percentage", "Display the percentage along with the time with the scrubber", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(DisplaySecondaryFormat, "Secondary", "Display the time or frame count as a secondary format along with the scrubber", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(SnapToFrames, "Frames", "Snap to frame boundaries", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(SnapToNotifies, "Notifies", "Snap to notifies and notify states", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(SnapToMontageSections, "Montage Sections", "Snap to montage sections", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(SnapToCompositeSegments, "Composite Segments", "Snap to composite segments", EUserInterfaceActionType::ToggleButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE