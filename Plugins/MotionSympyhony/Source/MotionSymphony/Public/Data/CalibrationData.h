//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Objects/Assets/MotionCalibration.h"
#include "GameplayTagContainer.h"
#include "CalibrationData.generated.h"

struct FJointWeightSet;
struct FTrajectoryWeightSet;
class UMotionDataAsset;
class UMotionMatchConfig;
class UMotionCalibration;

/** A data structure containing weightings and multipliers for specific motion
matching aspects. Motion Matching distance costs are multiplied by these 
weights where relevant to calibrate the animation data.*/
USTRUCT(BlueprintType)
struct MOTIONSYMPHONY_API FCalibrationData
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY()
	TArray<float> Weights;

public:
	FCalibrationData();
	FCalibrationData(UMotionDataAsset* SourceMotionData);
	FCalibrationData(UMotionMatchConfig* SourceConfig);
	FCalibrationData(const int32 AtomCount);

	void Initialize(const int32 AtomCount);
	void Initialize(UMotionMatchConfig* SourceConfig);
	bool IsValidWithConfig(const UMotionMatchConfig* MotionConfig);

	void GenerateStandardDeviationWeights(const UMotionDataAsset* SourceMotionData, const FGameplayTagContainer& MotionTags);
	void GenerateStandardDeviationWeights(const TArray<float>& PoseMatrix, UMotionMatchConfig* InMMConfig);
	void GenerateFinalWeights(UMotionMatchConfig* MotionMatchConfig, const FCalibrationData& StdDeviationNormalizers);
};