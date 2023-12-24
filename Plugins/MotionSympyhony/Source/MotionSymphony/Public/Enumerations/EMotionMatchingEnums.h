//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EMotionMatchingEnums.generated.h"

/** An enumeration for all modes or 'states' that the Motion Matching NOde can be in.*/
UENUM(BlueprintType)
enum class EMotionMatchingMode : uint8
{
	MotionMatching,
	DistanceMatching,
	Action
};

/** An enumeration used to flag poses with how they interact with the search matrix*/
UENUM()
enum class EPoseSearchFlag : uint8
{
	Searchable, //A normal pose that is searchable. It exists in the pose database and the pose matrix
	NextNatural, //A pose that can only be searched if some predecessor pose is currently playing. Exists in the pose database but not the matrix. Pose exists in a next natural database instead
	EdgePose, //A pose that cannot be searched but can be played by time moving forward. An edge pose being played results in a forced search. Exists in the pose database but not the pose matrix.
	DoNotUse, //A pose that cannot be searched. Gets removed from both the pose database and the pose matrix
};

/** An enumeration for the blend status of any given motion matching animation channel */
UENUM(BlueprintType)
enum class EBlendStatus : uint8
{
	Inactive,
	Chosen,
	Dominant,
	Decay
};

/** An enumeration for the different methods of transitioning between animations in motion matching */
UENUM(BlueprintType)
enum class ETransitionMethod : uint8
{
	None,
	Inertialization
};

/** An enumeration for the different methods of determining past trajectory for motion matching */
UENUM(BlueprintType)
enum class EPastTrajectoryMode : uint8
{
	ActualHistory,
	CopyFromCurrentPose
};

/** An enumeration defining the different types of animation assets that can be used as motion matching source animations */
UENUM(BlueprintType)
enum class EMotionAnimAssetType : uint8
{
	None,
	Sequence,
	BlendSpace,
	Composite
};

UENUM(BlueprintType)
enum class EMotionMatchingSearchQuality : uint8
{
	Performance UMETA(ToolTip = "The standard motion matching search algorithm"),
	Quality UMETA(DisplayName = "Quality (Experimental)", ToolTip = "Adds additional search complexity for better quality at a small performance cost. For this mode to work your Motion Config must contain two 'Bone Location and Velocity' featuers at the end of the list, preferably for the feet.")
};

/** An enumeration defining the different behaviour modes for trajectory generators */
UENUM(BlueprintType)
enum class ETrajectoryMoveMode : uint8
{
	Standard,
	Strafe
};

UENUM(BlueprintType)
enum class ETrajectoryModel : uint8
{
	Spring,
	UECharacterMovement
};

/** An enumeration defining the control modes for the trajectory generator. i.e. whether it is player controlled or AI controlled*/
UENUM(BlueprintType)
enum class ETrajectoryControlMode : uint8
{
	PlayerControlled,
	AIControlled
};