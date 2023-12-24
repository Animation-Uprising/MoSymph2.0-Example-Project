//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Enumerations/EMotionMatchingEnums.h"
#include "PoseMotionData.generated.h"

/** A data structure representing a single pose within an animation set. These poses are recorded at specific 
time intervals (usually 0.05s - 0.1s) and stored in a linear list. The list is searched continuously, either linearly or 
via an optimised method, for the best matching pose for the next frame. */
USTRUCT(BlueprintType)
struct MOTIONSYMPHONY_API FPoseMotionData
{
	GENERATED_USTRUCT_BODY()

public:
	/** The Id of this pose in the pose database */
	UPROPERTY(BlueprintReadOnly, Category = "MotionSymphony|Pose")
	int32 PoseId;

	/** The type of animation this pose uses */
	UPROPERTY(BlueprintReadOnly, Category = "MotionSymphony|Pose")
	EMotionAnimAssetType AnimType;

	/** The Id of the animation this pose relates to */
	UPROPERTY(BlueprintReadOnly, Category = "MotionSymphony|Pose")
	int32 AnimId = 0;
	
	/** The time within the referenced animation that this pose relates to*/
	UPROPERTY(BlueprintReadOnly, Category = "MotionSymphony|Pose")
	float Time = 0.0f;

	/** The position within a blend space that the pose exists*/
	UPROPERTY(BlueprintReadOnly, Category = "MotionSymphony|Pose")
	FVector2D BlendSpacePosition = FVector2D::ZeroVector;

	/** Id of the next pose in the animation database*/
	UPROPERTY(BlueprintReadOnly, Category = "MotionSymphony|Pose")
	int32 NextPoseId;

	/** Id of the previous pose in the animation database*/
	UPROPERTY(BlueprintReadOnly, Category = "MotionSymphony|Pose")
	int32 LastPoseId;

	/** Is this pose for a mirrored version of the animation */
	UPROPERTY(BlueprintReadOnly, Category = "MotionSymphony|Pose")
	bool bMirrored = false;

	/** If true this pose will not be searched or jumped to in the pre-process phase */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MotionSymphony|Pose")
	EPoseSearchFlag SearchFlag = EPoseSearchFlag::Searchable;

	UPROPERTY(BlueprintReadWrite, Category = "MotionSymphony|Pose")
	FGameplayTagContainer MotionTags;

public:
	FPoseMotionData();
	FPoseMotionData(int32 InPoseId, EMotionAnimAssetType InAnimType, int32 InAnimId, float InTime,
		EPoseSearchFlag InPoseSearchFlag, bool bInMirrored, const FGameplayTagContainer& InMotionTags);
		
		
	
	void Clear();

	// FPoseMotionData& operator += (const FPoseMotionData& rhs);
	// FPoseMotionData& operator /= (const float rhs);
	// FPoseMotionData& operator *= (const float rhs);
};