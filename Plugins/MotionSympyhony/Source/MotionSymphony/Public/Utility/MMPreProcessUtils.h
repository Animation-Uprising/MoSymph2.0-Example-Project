//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimComposite.h"
#include "BoneContainer.h"
#include "Enumerations/EMotionMatchingEnums.h"
#include "Enumerations/EMMPreProcessEnums.h"
#include "Math/UnrealMathUtility.h"

class UMotionDataAsset;
class USkeletalMeshComponent;
struct FTrajectory;
struct FTrajectoryPoint;
struct FJointData;

/** Utility class holding functions for motion matching pre-processing */
class MOTIONSYMPHONY_API FMMPreProcessUtils
{
public:
	/** Extract root motion parameters from blend sample data (usually used for extracting root motion from blend spaces */
	static void ExtractRootMotionParams(FRootMotionMovementParams& OutRootMotion, const TArray<FBlendSampleData>& BlendSampleData,
		const float BaseTime, const float DeltaTime, const bool AllowLooping);

	/** Extracts root motion velocity and rotational velocity of an animation at a given time and delta time */
	static void ExtractRootVelocity(FVector& OutRootVelocity, float& OutRootRotVelocity, UAnimSequence* AnimSequence, 
		const float Time, const float PoseInterval);

	/** Extracts root motion velcocity and rotational velocity of BlendSampleData (usually used for blend spaces) at a given time and delta time */
	static void ExtractRootVelocity(FVector& OutRootVelocity, float& OutRootRotVelocity, 
		const TArray<FBlendSampleData>& BlendSampleData, const float Time, const float PoseInterval);

	static void ExtractRootVelocity(FVector& OutRootVelocity, float& OutRootRotVelocity,
		UAnimComposite* AnimComposite, const float Time, const float PoseInterval);

	/** Extracts a past trajectory point for an animation */
	static void ExtractPastTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, UAnimSequence* AnimSequence, 
		const float BaseTime, const float PointTime, ETrajectoryPreProcessMethod PastMethod, 
	    UAnimSequence* PrecedingMotion);

	/** Extracts a past trajectory point for BlendSampleData (usually used for blend spaces) */
	static void ExtractPastTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, const TArray<FBlendSampleData>& BlendSampleData,
		const float BaseTime, const float PointTime, ETrajectoryPreProcessMethod PastMethod, UAnimSequence* PrecedingMotion);

	/** Extracts a past trajectory point from an animation composite */
	static void ExtractPastTrajectoryPoint(FTrajectoryPoint& OutTrajPoint,UAnimComposite* AnimComposite,
		const float BaseTime, const float PointTime, ETrajectoryPreProcessMethod PastMethod, UAnimSequence* PrecedingMotion);

	/** Extracts a future trajectory point for an animation */
	static void ExtractFutureTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, UAnimSequence* AnimSequence,
		const float BaseTime, const float PointTime, ETrajectoryPreProcessMethod FutureMethod, UAnimSequence* FollowingMotion);

	/** Extracts a future trajectory point for BlendSampleData (usually used for blend spaces) */
	static void ExtractFutureTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, const TArray<FBlendSampleData>& BlendSampleData,
		const float BaseTime, const float PointTime, ETrajectoryPreProcessMethod FutureMethod, UAnimSequence* FollowingMotion);

	/** Extracts a future trajectory point from an animation composite */
	static void ExtractFutureTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, UAnimComposite* AnimComposite,
		const float BaseTime, const float PointTime, ETrajectoryPreProcessMethod FutureMethod, UAnimSequence* FollowingMotion);

	/** Extracts a looping trajectory point given time and trajectory point time horizon */
	static void ExtractLoopingTrajectoryPoint(FTrajectoryPoint& OutTrajPoint,  UAnimSequence* AnimSequence,
		const float BaseTime, const float PointTime);

	/** Extracts a looping trajectory point for BlendSampleData (usually for blend spaces) given time and a trajectory point time horizon */
	static void ExtractLoopingTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, const TArray<FBlendSampleData>& BlendSampleData, 
		const float BaseTime, const float PointTime);

	static void ExtractLoopingTrajectoryPoint(FTrajectoryPoint& OutTrajPoint,UAnimComposite* AnimComposite,
		const float BaseTime, const float PointTime);
	
	/** Extracts data for a single joint from an animation at a given time and delta time */
	static void ExtractJointData(FJointData& OutJointData, UAnimSequence* AnimSequence,
		const int32 JointId, const float Time, const float PoseInterval);

	/** Extracts data for a single joint from BlendSampleData (usually used for blend spaces) at a given time and delta time */
	static void ExtractJointData(FJointData& OutJointData, const TArray<FBlendSampleData>& BlendSampleData,
		const int32 JointId, const float Time, const float PoseInterval);

	/** Extracts data for a single joint from an animation composite at a given time and delta time */
	static void ExtractJointData(FJointData& OutJointData, UAnimComposite* AnimComposite,
		const int32 JointId, const float Time, const float PoseInterval);

	/** Extracts data for a single joint (via Bone Reference) from an animation at a given time and delta time */
	static void ExtractJointData(FJointData& OutJointData, UAnimSequence* AnimSequence,
		const FBoneReference& BoneReference, const float Time, const float PoseInterval);

	/** Extracts data for a single joint (via Bone Reference) from an animation at a given time and delta time */
	static void ExtractJointData(FJointData& OutJointData, const TArray<FBlendSampleData>& BlendSampleData,
		const FBoneReference& BoneReference, const float Time, const float PoseInterval);

	/** Extracts data for a single joint (via Bone Reference) from an animation composite at a given time and delta time */
	static void ExtractJointData(FJointData& OutJointData, UAnimComposite* AnimComposite,
		const FBoneReference& BoneReference, const float Time, const float PoseInterval);

	/** Extracts a joint transform relative to the character root from an animation (via joint Id) */
	static void GetJointTransform_RootRelative(FTransform& OutJointTransform, UAnimSequence* AnimSequence,
		const int32 JointId, const float Time);

	/** Extracts a joint transform relative to the character root from BlendSampleData (via joint Id) */
	static void GetJointTransform_RootRelative(FTransform& OutJointTransform, const TArray<FBlendSampleData>& BlendSampleData,
		const int32 JointId, const float Time);

	/** Extracts a joint transform relative to the character root from an animation composite (via joint Id) */
	static void GetJointTransform_RootRelative(FTransform& OutJointTransform, UAnimComposite* AnimComposite,
		const int32 JointId, const float Time);

	/** Extracts a joint transform relative to the character root from an animation (via BonesToRoot array */
	static void GetJointTransform_RootRelative(FTransform& OutTransform, UAnimSequence* AnimSequence,
		const TArray<FName>& BonesToRoot, const float Time);

	/** Extracts a joint transform relative to the character root from a BlendSampleData (via BonesToRoot array) */
	static void GetJointTransform_RootRelative(FTransform& OutTransform, const TArray<FBlendSampleData>& BlendSampleData,
		const TArray<FName>& BonesToRoot, const float Time);

	/** Extracts a joint transform relative to the character root from an animation composite (via BonesToRoot array */
	static void GetJointTransform_RootRelative(FTransform& OutTransform, UAnimComposite* AnimComposite,
		const TArray<FName>& BonesToRoot, const float Time);

	/** Extracts a joint velocity relative to the character root from an animation (via Joint Id) */
	static void GetJointVelocity_RootRelative(FVector& OutJointVelocity, UAnimSequence* AnimSequence, 
		const int32 JointId, const float Time, const float PoseInterval);

	/** Extracts a joint velocity relative to the character root from an animation (via Joint Id) */
	static void GetJointVelocity_RootRelative(FVector& OutJointVelocity, const TArray<FBlendSampleData>& BlendSampleData,
		const int32 JointId, const float Time, const float PoseInterval);

	/** Extracts a joint velocity relative to the character root from an animation composite (via Joint Id) */
	static void GetJointVelocity_RootRelative(FVector& OutJointVelocity, UAnimComposite* AnimComposite,
		const int32 JointId, const float Time, const float PoseInterval);

	/** Extracts a joint velocity relative to the character root from an animation (via BonesToRoot array) */
	static void GetJointVelocity_RootRelative(FVector& OutJointVelocity, UAnimSequence* AnimSequence,
		const TArray<FName>& BonesToRoot, const float Time, const float PoseInterval);

	/** Extracts a joint velocity relative to the character root from a BlendSampleData (via BonesToRoot array) */
	static void GetJointVelocity_RootRelative(FVector& OutJointVelocity, const TArray<FBlendSampleData>& BlendSampleData,
		const TArray<FName>& BonesToRoot, const float Time, const float PoseInterval);

	/** Extracts a joint velocity relative to the character root from an animation composite(via BonesToRoot array) */
	static void GetJointVelocity_RootRelative(FVector& OutJointVelocity, UAnimComposite* AnimComposite,
		const TArray<FName>& BonesToRoot, const float Time, const float PoseInterval);

	/** Converts a BoneId from a reference skeleton to a bone Id on an animation sequence by the bone name */
	static int32 ConvertRefSkelBoneIdToAnimBoneId(const int32 BoneId, 
		const FReferenceSkeleton& FromRefSkeleton, const UAnimSequence* ToAnimSequence);

	/** Converts a bone name into a bone id for an animatino sequence*/
	static int32 ConvertBoneNameToAnimBoneId(const FName BoneName, const UAnimSequence* ToAnimSequence);

	/** Finds path from a bone all the way to it's root*/
	static void FindBonePathToRoot(const UAnimSequenceBase* AnimationSequenceBase, FName BoneName, TArray<FName>& BonePath);

	static FName FindMirrorBoneName(const USkeleton* InSkeleton, UMirrorDataTable* InMirrorDataTable,
	                                FName BoneName);

	///** Checks if the specified time on an animation is tagged with DoNotUse */
	//static bool GetDoNotUseTag(const UAnimSequence* AnimSequence, const float Time, const float PoseInterval);

	///**Checks if the specified time on BlendSampleData (usually used for blend spaces is tagged with DoNotUse */
	//static bool GetDoNotUseTag(const TArray<FBlendSampleData> BlendSampleData, const float Time, 
	//	const float PoseInterval, const bool HighestWeightOnly = true);
};
