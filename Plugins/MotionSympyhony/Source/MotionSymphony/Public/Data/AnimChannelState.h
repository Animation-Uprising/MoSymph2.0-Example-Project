//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PoseMotionData.h"
#include "Enumerations/EMotionMatchingEnums.h"
#include "MotionSymphony.h"
#include "Animation/AnimationAsset.h"
#include "AnimChannelState.generated.h"

/** A data structure for tracking animation channels within a motion matching animation stack. 
Every time a new pose is picked from the animation database, a channel is created for managing
its state. */
USTRUCT(BlueprintType)
struct MOTIONSYMPHONY_API FAnimChannelState
{
	GENERATED_USTRUCT_BODY()

public:
	/** Id of the animation used for this channel */
	UPROPERTY()
	int32 AnimId;

	/** Type of the animation used for this channel */
	UPROPERTY()
	EMotionAnimAssetType AnimType;

	/** Id of the pose that started this channel*/
	UPROPERTY()
	int32 StartPoseId;

	/** Start time within the animation that this channel started*/
	UPROPERTY()
	float StartTime;

	/** Position within the blend space that this channel uses (if it is a blend space channel) */
	UPROPERTY()
	FVector2D BlendSpacePosition;

	/** Current time of the animation in this channel */
	UPROPERTY()
	float AnimTime;

	/** Does the animation in this channel loop? */
	UPROPERTY()
	bool bLoop;

	/** The current play rate for this anim channel state */
	UPROPERTY()
	float PlayRate;

	/** Is the animation for this channel mirrored */
	UPROPERTY()
	bool bMirrored;

	/** The length of the animation in this channel */
	UPROPERTY()
	float AnimLength;

	/** Blend sample data cache used for blend spaces*/
	TArray<FBlendSampleData> BlendSampleDataCache;

	/** The cached triangulation index for blendspaces to improve performance */
	UPROPERTY();
	int32 CachedTriangulationIndex;

public:
	void Update(const float DeltaTime, const float NodePlayRate);

	FAnimChannelState();
	FAnimChannelState(const FPoseMotionData& InPose, float InAnimLength, bool bInLoop = false,
		float InPlayRate = 1.0f, bool bInMirrored = false, float InTimeOffset=0.0f, float InPoseOffset=0.0f);
};