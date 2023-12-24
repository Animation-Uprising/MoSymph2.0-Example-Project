//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Components/DistanceMatching.h"
#include "UObject/NameTypes.h"
#include "AnimNode_MSDistanceMatching.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FAnimNode_MSDistanceMatching : public FAnimNode_SequencePlayer
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatching", meta = (PinShownByDefault))
	float DesiredDistance;
	
	/** The name of the distance curve to use for this node*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatching")
	FName DistanceCurveName;

	/** If checked the distance curve values will be negated. MoSymph uses +ve values if the marker is ahead and -ve values
	if the marker is behind. If your animation curves are opposite to this you may need to toggle this option on*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatching")
	bool bNegateDistanceCurve;

	/** The type of distance matching movement. Forward, Backward or Both*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatching")
	EDistanceMatchType MovementType;

	/** A limit for distance matching. Once the limit is exceeded the animation continues to play as normal */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatching")
	float DistanceLimit;

	/** The distance under which it would be considered that a destination is reached. I.e. the distance is close enough to 
	be considered zero for a 'forward' type of distance matching.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	float DestinationReachedThreshold;

	/** The rate at which distance matching animations are smoothed. If the value is < 0.0f then smoothing is disabled*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta = (ClampMin = -1.0f, ClampMax = 1.0f))
	float SmoothRate;

	/** A time threshold to enable and disable smoothing. If the time jump between the current time and the desired distance time
	is above this value then smoothing will be disabled and the animation will jump instantly to the desired time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	float SmoothTimeThreshold;

protected:
	bool bInitialized = false;

private:
	FDistanceMatchingModule DistanceMatchingModule;

	UPROPERTY(Transient)
	UAnimSequenceBase* LastAnimSequenceUsed;

public:
	FAnimNode_MSDistanceMatching();

protected:
	// FAnimNode_Base interface
	virtual void InitializeData();
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	// End of FAnimNode_Base interface
};