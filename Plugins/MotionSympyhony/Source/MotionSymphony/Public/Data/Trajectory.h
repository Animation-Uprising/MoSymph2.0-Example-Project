//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TrajectoryPoint.h"
#include "Trajectory.generated.h"

USTRUCT(BlueprintType)
struct MOTIONSYMPHONY_API FTrajectory
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trajectory")
	TArray<FTrajectoryPoint>  TrajectoryPoints;

public:
	FTrajectory();
	~FTrajectory();

public: 
	void Initialize(int TrajectoryCount);
	void Clear();

	void MakeRelativeTo(FTransform Transform);

	void SetTrajectoryPoint(const int32 Index, const FVector InPosition, const float InRotationZ);
	void AddTrajectoryPoint(const FVector InPosition, const float InRotationZ);
	int32 TrajectoryPointCount() const;
};

USTRUCT(BlueprintType)
struct MOTIONSYMPHONY_API FMotionMatchingInputData
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "Trajectory")
	TArray<float> DesiredInputArray;
	
	void Empty(const int32 Size);

	FMotionMatchingInputData();
	~FMotionMatchingInputData();
};