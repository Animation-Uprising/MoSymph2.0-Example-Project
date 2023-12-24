//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Tags/Tag_NextNatural.h"

UTag_NextNatural::UTag_NextNatural(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor::Orange;
#endif
}

void UTag_NextNatural::PreProcessTag(TObjectPtr<UMotionAnimObject> OutMotionAnim,
                                     UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime) 
{
	Super::PreProcessTag(OutMotionAnim, OutMotionData, StartTime, EndTime);
}

void UTag_NextNatural::PreProcessPose(FPoseMotionData& OutPose, TObjectPtr<UMotionAnimObject> OutMotionAnim,
                                      UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime)
{
	Super::PreProcessPose(OutPose, OutMotionAnim, OutMotionData, StartTime, EndTime);
	OutPose.SearchFlag = EPoseSearchFlag::NextNatural;
}