//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/DistanceMatching.h"
#include "UObject/NameTypes.h"
#include "DistanceMatchingNodeData.generated.h"

UENUM(BlueprintType)
enum class EDistanceMatchingUseCase : uint8
{
	None,
	Strict
};

USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FDistanceMatchingNodeData
{
	GENERATED_BODY()

public:	
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatching")
	float DestinationReachedThreshold;

	/** The rate at which distance matching animations are smoothed. If the value is < 0.0f then smoothing is disabled*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatching", meta = (ClampMin = -1.0f, ClampMax = 1.0f))
	float SmoothRate;

	/** A time threshold to enable and disable smoothing. If the time jump between the current time and the desired distance time
	is above this value then smoothing will be disabled and the animation will jump instantly to the desired time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatching")
	float SmoothTimeThreshold;

public:
	FDistanceMatchingNodeData();

	float GetDistanceMatchingTime(FDistanceMatchingModule* InDistanceModule, float InDesiredDistance, float CurrentTime) const;
};
