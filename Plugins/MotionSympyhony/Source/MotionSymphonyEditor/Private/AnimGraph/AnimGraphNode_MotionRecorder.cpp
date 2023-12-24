//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimGraphNode_MotionRecorder.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_MotionRecorder"


FLinearColor UAnimGraphNode_MotionRecorder::GetNodeTitleColor() const
{
	return FLinearColor(0.0f, 0.1f, 0.2f);
}

FText UAnimGraphNode_MotionRecorder::GetTooltipText() const
{
	return LOCTEXT("NodeToolTip", "Motion Snapshot");
}

FText UAnimGraphNode_MotionRecorder::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("NodeTitle", "Motion Snapshot");
}

FText UAnimGraphNode_MotionRecorder::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "Motion Symphony");
}


#undef LOCTEXT_NAMESPACE