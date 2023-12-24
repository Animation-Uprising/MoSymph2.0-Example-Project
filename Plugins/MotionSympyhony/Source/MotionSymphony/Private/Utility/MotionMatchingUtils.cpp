//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Utility/MotionMatchingUtils.h"

#include "AnimNode_MSMotionMatching.h"
#include "Objects/Assets/MotionCalibration.h"
#include "Data/CalibrationData.h"
#include "BonePose.h"

void FMotionMatchingUtils::LerpFloatArray(TArray<float>& OutLerpArray, const float* FromArrayPtr, const float* ToArrayPtr,
                                          float Progress)
{
	for(int32 i = 0; i < OutLerpArray.Num(); ++i)
	{
		OutLerpArray[i] = FMath::Lerp(*FromArrayPtr, *ToArrayPtr, Progress);
		++FromArrayPtr;
		++ToArrayPtr;
	}
}

void FMotionMatchingUtils::LerpPose(FPoseMotionData& OutLerpPose, const FPoseMotionData& From,
	const FPoseMotionData& To, const float Progress)
{
	if (Progress < 0.5f)
	{
		OutLerpPose.PoseId = From.PoseId;
		OutLerpPose.AnimType = From.AnimType;
		OutLerpPose.AnimId = From.AnimId;
		OutLerpPose.BlendSpacePosition = From.BlendSpacePosition;
		OutLerpPose.bMirrored = From.bMirrored;
		OutLerpPose.SearchFlag = From.SearchFlag;
		OutLerpPose.MotionTags = From.MotionTags;
		
	}
	else
	{
		OutLerpPose.PoseId = To.PoseId;
		OutLerpPose.AnimType = To.AnimType;
		OutLerpPose.AnimId = To.AnimId;
		OutLerpPose.BlendSpacePosition = To.BlendSpacePosition;
		OutLerpPose.bMirrored = To.bMirrored;
		OutLerpPose.SearchFlag = To.SearchFlag;
		OutLerpPose.MotionTags = To.MotionTags;
	}

	OutLerpPose.LastPoseId = From.PoseId;
	OutLerpPose.NextPoseId = To.PoseId;
	OutLerpPose.Time = FMath::Lerp(From.Time, To.Time, Progress);
}

void FMotionMatchingUtils::LerpLinearPoseData(TArray<float>& OutLerpPose, TArray<float> From, TArray<float> To,
                                              const float Progress)
{
	const int32 Count = FMath::Min3(OutLerpPose.Num(), From.Num(), To.Num());
	for(int32 i = 0; i < Count; ++i)
	{
		OutLerpPose[i] = FMath::Lerp(From[i], To[i], Progress);
	}
}

void FMotionMatchingUtils::LerpLinearPoseData(TArray<float>& OutLerpPose, float* From, float* To, const float Progress,
                                              const int32 PoseSize)
{
	const int32 Count = FMath::Min(OutLerpPose.Num(), PoseSize);
	for(int32 i = 0; i < Count; ++i)
	{
		OutLerpPose[i] = FMath::Lerp(*From, *To, Progress);
		++From;
		++To;
	}
	
}

float FMotionMatchingUtils::ComputeTrajectoryCost(const TArray<FTrajectoryPoint>& Current,
                                                  const TArray<FTrajectoryPoint>& Candidate, const float PosWeight, const float rotWeight)
{
	float Cost = 0.0f;

	for (int32 i = 0; i < Current.Num(); ++i)
	{
		const FTrajectoryPoint& CurrentPoint = Current[i];
		const FTrajectoryPoint& CandidatePoint = Candidate[i];

		//Cost of distance between trajectory points
		Cost += FVector::DistSquared(CandidatePoint.Position, CurrentPoint.Position) * PosWeight;

		//Cost of angle between trajectory point facings
		Cost += FMath::Abs(FMath::FindDeltaAngleDegrees(CandidatePoint.RotationZ, CurrentPoint.RotationZ)) * rotWeight;
	}

	return Cost;
}

float FMotionMatchingUtils::ComputePoseCost(const TArray<FJointData>& Current, const TArray<FJointData>& Candidate,
	const float PosWeight, const float VelWeight)
{
	float Cost = 0.0f;

	for (int32 i = 0; i < Current.Num(); ++i)
	{
		const FJointData& CurrentJoint = Current[i];
		const FJointData& CandidateJoint = Candidate[i];

		//Cost of velocity comparison
		Cost += FVector::DistSquared(CurrentJoint.Velocity, CandidateJoint.Velocity) * VelWeight;

		//Cost of distance between joints
		Cost += FVector::DistSquared(CurrentJoint.Position, CandidateJoint.Position) * PosWeight;
	}

	return Cost;
}



float FMotionMatchingUtils::SignedAngle(const FVector From, const FVector To, const FVector Axis)
{
	const float UnsignedAngle = FMath::Acos(FVector::DotProduct(From, To));

	const float CrossX = From.Y * To.Z - From.Z * To.Y;
	const float CrossY = From.Z * To.X - From.X * To.Z;
	const float CrossZ = From.X * To.Y - From.Y * To.X;

	const float Sign = FMath::Sign(Axis.X * CrossX + Axis.Y * CrossY + Axis.Z * CrossZ);

	return UnsignedAngle * Sign;
}

float FMotionMatchingUtils::GetFacingAngleOffset(EAllAxis CharacterForward)
{
	switch(CharacterForward)
	{
		case EAllAxis::X: return 0.0f;
		case EAllAxis::Y: return 90.0f;
		case EAllAxis::NegX: return 180.0f;
		case EAllAxis::NegY: return -90.0f;
		default: return 0.0f;
	}
}


float FMotionMatchingUtils::WrapAnimationTime(float time, float length)
{
	if (time < 0.0f)
	{
		const int32 wholeNumbers = FMath::FloorToInt(FMath::Abs(time) / length);
		time = length + (time + (wholeNumbers * length));
	}
	else if (time > length)
	{
		const int32 wholeNumbers = FMath::FloorToInt(time / length);
		time = time - (wholeNumbers * length);
	}

	return time;
}
