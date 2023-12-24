//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Tags/Tag_DoNotUse.h"

UTag_DoNotUse::UTag_DoNotUse(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor::Red;
#endif
}

void UTag_DoNotUse::PreProcessTag(TObjectPtr<UMotionAnimObject> OutMotionAnim,
                                  UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime) 
{
	Super::PreProcessTag(OutMotionAnim, OutMotionData, StartTime, EndTime);
}

void UTag_DoNotUse::PreProcessPose(FPoseMotionData& OutPose, TObjectPtr<UMotionAnimObject> OutMotionAnim,
                                   UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime)
{
	Super::PreProcessPose(OutPose, OutMotionAnim, OutMotionData, StartTime, EndTime);
	OutPose.SearchFlag = EPoseSearchFlag::DoNotUse;
}