//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimChannelState.h"
#include "MotionSymphony.h"

FAnimChannelState::FAnimChannelState()
	: AnimId(0), 
	  AnimType(EMotionAnimAssetType::None),
	  StartPoseId(0), 
	  StartTime(0.0f), 
	  BlendSpacePosition(FVector2D(0.0f)),
	  AnimTime(0.0f), 
	  bLoop(false),
	  PlayRate(1.0f),
	  bMirrored(false),
	  AnimLength(0.0f),
	  CachedTriangulationIndex(-1)
{ 
}

FAnimChannelState::FAnimChannelState(const FPoseMotionData & InPose, float InAnimLength, bool bInLoop,
	float InPlayRate, bool bInMirrored, float InTimeOffset /*= 0.0f*/, float InPoseOffset /*= 0.0f*/)
	: AnimId(InPose.AnimId), 
	AnimType(InPose.AnimType),
	StartPoseId(InPose.PoseId),
	StartTime(InPose.Time),
	BlendSpacePosition(InPose.BlendSpacePosition),
	AnimTime(InPose.Time + InTimeOffset + InPoseOffset), 
	bLoop(bInLoop),
	PlayRate(InPlayRate),
	bMirrored(bInMirrored),
	AnimLength(InAnimLength),
	CachedTriangulationIndex(-1)
{
	if(AnimTime > AnimLength)
	{
		AnimTime = AnimLength;
	}
}

void FAnimChannelState::Update(const float DeltaTime,
                               const float NodePlayRate)
{
	AnimTime += DeltaTime * PlayRate * NodePlayRate;
	
	if (bLoop && AnimTime > AnimLength)
	{
		const float WrapAmount = (AnimTime / AnimLength);
		AnimTime = (WrapAmount - FMath::Floor(WrapAmount)) * AnimLength;
	}
}