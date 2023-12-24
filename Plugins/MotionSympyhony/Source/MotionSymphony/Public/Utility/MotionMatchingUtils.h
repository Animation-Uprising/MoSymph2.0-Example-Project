//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "EMMPreProcessEnums.h"
#include "JointData.h"
#include "PoseMotionData.h"
#include "TrajectoryPoint.h"
#include "Data/PoseMatrix.h"
#include "Math/UnrealMathUtility.h"

struct FMotionMatchingInputData;
struct FTrajectory;
struct FCompactPose;
class UMirroringProfile;
class USkeletalMeshComponent;
struct FAnimMirroringData;
struct FCalibrationData;

class MOTIONSYMPHONY_API FMotionMatchingUtils
{
public:
	static void LerpFloatArray(TArray<float>& OutLerpArray, const float* FromArrayPtr, const float* ToArrayPtr, float Progress);
	
	static void LerpPose(FPoseMotionData& OutLerpPose, const FPoseMotionData& From, const FPoseMotionData& To, float Progress);

	static void LerpLinearPoseData(TArray<float>& OutLerpPose, TArray<float> From, TArray<float> To, const float Progress);

	static void LerpLinearPoseData(TArray<float>& OutLerpPose, float* From, float* To, const float Progress, const int32 PoseSize);

	static float WrapAnimationTime(float Time, float Length);

	static float ComputeTrajectoryCost(const TArray<FTrajectoryPoint>& Current, 
		const TArray<FTrajectoryPoint>& Candidate, const float PosWeight, const float RotWeight);
	
	static float ComputePoseCost(const TArray<FJointData>& Current,
		const TArray<FJointData>& Candidate, const float PosWeight,
		const float VelWeight);

	static inline float LerpAngle(float AngleA, float AngleB, float Progress)
	{
		const float Max = PI * 2.0f;
		const float DeltaAngle = fmod((AngleB - AngleA), Max);

		return AngleA + (fmod(2.0f * DeltaAngle, Max) - DeltaAngle) * Progress;
	}

	static float SignedAngle(const FVector From, const FVector To, const FVector Axis);

	static float GetFacingAngleOffset(EAllAxis CharacterForward);
};