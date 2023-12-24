//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Tags/TagSection.h"

// UTagSection::UTagSection(const FObjectInitializer& ObjectInitializer)
// 	: Super(ObjectInitializer)
// {
//
// }

void UTagSection::PreProcessTag(TObjectPtr<UMotionAnimObject> OutMotionAnim, UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime)
{
	Received_PreProcessTag(OutMotionAnim, OutMotionData, StartTime, EndTime);
}

void UTagSection::PreProcessPose(FPoseMotionData& OutPose, TObjectPtr<UMotionAnimObject> OutMotionAnim, UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime)
{
	Received_PreProcessPose(OutPose, OutMotionAnim, OutMotionData, StartTime, EndTime);
}

void UTagSection::CopyTagData(UTagSection* CopyTag)
{
}
