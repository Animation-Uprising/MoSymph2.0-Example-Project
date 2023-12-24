//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimGraphNode_MSFootLocker.h"

#define LOCTEXT_NAMESPACE "StriderEditor"

UAnimGraphNode_MSFootLocker::UAnimGraphNode_MSFootLocker()
{
}

FText UAnimGraphNode_MSFootLocker::GetTooltipText() const
{
	return LOCTEXT("FootLocker", "Foot Locker");
}

FLinearColor UAnimGraphNode_MSFootLocker::GetNodeTitleColor() const
{
	return FLinearColor(0.0f, 1.0f, 0.5f);
}

FText UAnimGraphNode_MSFootLocker::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("FootLocker", "Foot Locker");
}

FString UAnimGraphNode_MSFootLocker::GetNodeCategory() const
{
	return TEXT("Animation Warping");
}

const FAnimNode_SkeletalControlBase* UAnimGraphNode_MSFootLocker::GetNode() const
{
	return &FootLockerNode;
}

#undef LOCTEXT_NAMESPACE