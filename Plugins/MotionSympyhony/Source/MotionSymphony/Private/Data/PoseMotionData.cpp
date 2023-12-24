//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "PoseMotionData.h"
#include "MotionSymphony.h"

/**
 * @brief 
 */
FPoseMotionData::FPoseMotionData()
	: PoseId(0),
	AnimType(EMotionAnimAssetType::None),
	BlendSpacePosition(FVector2D(0.0f)),
	NextPoseId(0),
	LastPoseId(0)
{ 	
}

FPoseMotionData::FPoseMotionData(int32 InPoseId, EMotionAnimAssetType InAnimType, int32 InAnimId, float InTime,
	EPoseSearchFlag InPoseFlag, bool bInMirrored, const FGameplayTagContainer& InMotionTags)
		:PoseId(InPoseId), 
	  AnimType(InAnimType),
	  AnimId(InAnimId),
	  Time(InTime), 
	  BlendSpacePosition(FVector2D(0.0f)),
	  NextPoseId(InPoseId + 1),
	  LastPoseId(FMath::Clamp(InPoseId - 1, 0, InPoseId)), 
	  bMirrored(bInMirrored),
	  SearchFlag(InPoseFlag), 
	  MotionTags(InMotionTags)
{
}

void FPoseMotionData::Clear()
{
	PoseId = -1;
	AnimType = EMotionAnimAssetType::None;
	AnimId = -1;
	Time = 0;
	BlendSpacePosition = FVector2D(0.0f);
	NextPoseId = -1;
	LastPoseId = -1;
	bMirrored = false;
	SearchFlag = EPoseSearchFlag::Searchable;
	MotionTags = FGameplayTagContainer::EmptyContainer;
}