//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimGraph/DistanceMatchingNodeData.h"

#define LOCTEXT_NAMESPACE "MotionSymphonyNodes"

FDistanceMatchingNodeData::FDistanceMatchingNodeData()
	: DistanceCurveName(FName(TEXT("MoSymph_Distance"))),
	bNegateDistanceCurve(false),
	MovementType(EDistanceMatchType::None),
	DistanceLimit(-1.0f),
	DestinationReachedThreshold(5.0f),
	SmoothRate(-1.0f),
	SmoothTimeThreshold(0.15f)
{
	
}

float FDistanceMatchingNodeData::GetDistanceMatchingTime(FDistanceMatchingModule* InDistanceModule, float InDesiredDistance, float CurrentTime) const
{
	if(!InDistanceModule || !InDistanceModule->AnimSequence)
	{
		return -1.0f;
	}
	
	if(DistanceLimit < 0.0f || InDesiredDistance < DistanceLimit)
	{
		float Time = -1.0f;

		//Has the destination been reached?
		if(MovementType == EDistanceMatchType::Forward
			&& InDesiredDistance < DestinationReachedThreshold)
		{
			return -1.0f;
		}

		Time = InDistanceModule->FindMatchingTime(InDesiredDistance, bNegateDistanceCurve);
		
		if(Time > -0.5f)
		{
			if(SmoothRate > 0.0f && FMath::Abs(Time - CurrentTime) < SmoothTimeThreshold)
			{
				const float DesiredTime = FMath::Clamp(Time, 0.0f, InDistanceModule->AnimSequence->GetPlayLength());
				return FMath::Lerp(CurrentTime, DesiredTime, SmoothRate);
			}
			else
			{
				return FMath::Clamp(Time, 0.0f, InDistanceModule->AnimSequence->GetPlayLength());
			}
		}
		else //Could not find a time to match distance so continue playing as if it were a sequence player
			{
			return -1.0f;
			}
	}

	return -1.0f;
}

#undef LOCTEXT_NAMESPACE
