//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Data/CalibrationData.h"
#include "Objects/Assets/MotionDataAsset.h"
#include "Objects/Assets/MotionMatchConfig.h"

FCalibrationData::FCalibrationData()
{
	Weights.Empty(30);
}

FCalibrationData::FCalibrationData(UMotionDataAsset* SourceMotionData)
{
	if(!SourceMotionData)
	{
		Weights.Empty(30);
		return;
	}

	const UMotionMatchConfig* MMConfig = SourceMotionData->MotionMatchConfig;

	if(!MMConfig)
	{
		return;
	}
	
	Weights.SetNum(MMConfig->TotalDimensionCount);
}

FCalibrationData::FCalibrationData(UMotionMatchConfig* SourceConfig)
{
	Initialize(SourceConfig);
}

FCalibrationData::FCalibrationData(const int32 AtomCount)
{
	Weights.SetNum(FMath::Max(0, AtomCount));
}

void FCalibrationData::Initialize(const int32 AtomCount)
{
	Weights.SetNumZeroed(AtomCount);

	for(int32 i = 0; i < AtomCount; ++i)
	{
		Weights[i] = 1.0f;
	}
}

void FCalibrationData::Initialize(UMotionMatchConfig* SourceConfig)
{
	if (SourceConfig == nullptr)
	{
		Weights.Empty(30);
		return;
	}
	
	Weights.SetNumZeroed(SourceConfig->TotalDimensionCount);
	for(int32 i = 0; i < SourceConfig->TotalDimensionCount; ++i)
	{
		Weights[i] = 1.0f;
	}
}

bool FCalibrationData::IsValidWithConfig(const UMotionMatchConfig* MotionConfig)
{
	if (!MotionConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("FCalibrationData: Trying to call IsValidWithConfig(UMotionMatchConfig*) but the motion config passed is null"));
		return false;
	}
	
	if(Weights.Num() != MotionConfig->TotalDimensionCount)
	{
		UE_LOG(LogTemp, Error, TEXT("FCalibrationData: Calibration Data is invalid as the weight array does not match the motion config pose array"));
		return false;
	}

	return true;
}

void FCalibrationData::GenerateStandardDeviationWeights(const UMotionDataAsset* SourceMotionData, const FGameplayTagContainer& MotionTags)
{
	if (!SourceMotionData || !SourceMotionData->MotionMatchConfig)
	{
		return;
	}

	UMotionMatchConfig* MMConfig = SourceMotionData->MotionMatchConfig;
	Initialize(MMConfig);
	
	int32 SdPoseCount = 0;		//Sd means 'standard deviation'
	int32 LookupMatrixPoseId = 0;

	//Determine the total for each atom
	const int32 AtomCount = SourceMotionData->LookupPoseMatrix.AtomCount; //Actual number of matching atoms
	const int32 MeasuredAtomCount = AtomCount - 1; //Atom count excluding cost multiplier
	TArray<float> TotalsArray;
	TotalsArray.SetNumZeroed(MeasuredAtomCount);
	const TArray<float>& PoseArray = SourceMotionData->LookupPoseMatrix.PoseArray;
	for(const FPoseMotionData& Pose : SourceMotionData->Poses)
	{
		++LookupMatrixPoseId;
		
		if(Pose.SearchFlag == EPoseSearchFlag::Searchable
			|| Pose.MotionTags != MotionTags)
		{
			continue;
		}

		++SdPoseCount;
		
		const int32 PoseStartIndex = (LookupMatrixPoseId - 1) * AtomCount + 1; //+1 to make room for Pose Cost Multiplier
		for(int32 i = 0; i < MeasuredAtomCount; ++i)
		{
			TotalsArray[i] += PoseArray[PoseStartIndex + i];
		}
	}
	
	SdPoseCount = FMath::Max(SdPoseCount, 1);

	//Determine the Mean for each atom
	TArray<float> MeanArray;
	MeanArray.SetNumZeroed(MeasuredAtomCount);
	for(int32 i = 0; i < MeasuredAtomCount; ++i)
	{
		MeanArray[i] = TotalsArray[i] / SdPoseCount;
	}

	LookupMatrixPoseId = 0;

	//Determine the distance to the mean squared for each atom
	TArray<float> TotalDistanceSqrArray;
	TotalDistanceSqrArray.SetNumZeroed(MeasuredAtomCount);
	for (const FPoseMotionData& Pose : SourceMotionData->Poses)
	{
		++LookupMatrixPoseId;
		
		if(Pose.SearchFlag == EPoseSearchFlag::DoNotUse
			|| Pose.MotionTags != MotionTags)
		{
			continue;
		}
		
		const int32 PoseStartIndex = (LookupMatrixPoseId - 1) * AtomCount + 1; //+1 to make room for Pose Cost Multiplier
		int32 FeatureOffset = 0; //Since the standard deviation array does not include a pose cost multiplier the feature offset does not have a +1
		for(const TObjectPtr<UMatchFeatureBase> FeaturePtr : MMConfig->Features)
		{
			const int32 FeatureSize = FeaturePtr->Size();
			
			FeaturePtr->CalculateDistanceSqrToMeanArrayForStandardDeviations(TotalDistanceSqrArray,
				MeanArray, PoseArray, FeatureOffset, PoseStartIndex);
		
			FeatureOffset += FeatureSize;
		}
	}

	//Calculate the standard deviation and final standard deviation weight
	for(int32 i = 0; i < MeasuredAtomCount; ++i)
	{
		const float StandardDeviation = TotalDistanceSqrArray[i] / SdPoseCount;
		Weights[i] = FMath::IsNearlyZero(StandardDeviation)? 0.0f : 1.0f / StandardDeviation;
	}
}

void FCalibrationData::GenerateStandardDeviationWeights(const TArray<float>& PoseMatrix, UMotionMatchConfig* InMMConfig)
{
	if(InMMConfig == nullptr && PoseMatrix.Num() == 0)
	{
		return;
	}

	Initialize(InMMConfig);

	//Determine the total for each atom
	const int32 AtomCount = InMMConfig->TotalDimensionCount;
	TArray<float> TotalsArray;
	TotalsArray.SetNumZeroed(AtomCount);
	const int32 PoseCount = PoseMatrix.Num() / AtomCount;
	for(int32 PoseId = 0; PoseId < PoseCount; ++PoseId)
	{
		const int32 PoseStartIndex = PoseId * AtomCount; 
		for(int32 i = 0; i < AtomCount; ++i)
		{
			TotalsArray[i] += PoseMatrix[PoseStartIndex + i];
		}
	}

	//Determine the Mean for each atom
	TArray<float> MeanArray;
	MeanArray.SetNumZeroed(AtomCount);
	for(int32 i = 0; i < AtomCount; ++i)
	{
		MeanArray[i] = TotalsArray[i] / PoseCount;
	}

	//Determine the distance to the mean squared for each atom
	TArray<float> TotalDistanceSqrArray;
	TotalDistanceSqrArray.SetNumZeroed(AtomCount);
	//for (const FPoseMotionData& Pose : SourceMotionData->Poses)
	for(int32 PoseId = 0; PoseId < PoseCount; ++PoseId)
	{
		const int32 PoseStartIndex = PoseId * AtomCount; //+1 to make room for Pose Cost Multiplier
		int32 FeatureOffset = 0;
		for(const TObjectPtr<UMatchFeatureBase> FeaturePtr : InMMConfig->Features)
		{
			const int32 FeatureSize = FeaturePtr->Size();
			
			FeaturePtr->CalculateDistanceSqrToMeanArrayForStandardDeviations(TotalDistanceSqrArray,
				MeanArray, PoseMatrix, FeatureOffset, PoseStartIndex);
		
			FeatureOffset += FeatureSize;
		}
	}

	//Calculate the standard deviation and final standard deviation weight
	for(int32 i = 0; i < AtomCount; ++i)
	{
		const float StandardDeviation = TotalDistanceSqrArray[i] / PoseCount;
		Weights[i] = FMath::IsNearlyZero(StandardDeviation)? 0.0f : 1.0f / StandardDeviation;
	}
}

void FCalibrationData::GenerateFinalWeights(UMotionMatchConfig* MotionMatchConfig, const FCalibrationData& StdDeviationNormalizers)
{
	if (!MotionMatchConfig)
	{
		return;
	}
	
	Initialize(MotionMatchConfig);
	
	const float ResponseMultiplier = MotionMatchConfig->DefaultQualityVsResponsivenessRatio * 2.0f;
	const float QualityMultiplier = (1.0f - MotionMatchConfig->DefaultQualityVsResponsivenessRatio) * 2.0f;

	int32 AtomIndex = 0;
	TArray<TObjectPtr<UMatchFeatureBase>>& Features = MotionMatchConfig->Features;
	for(TObjectPtr<UMatchFeatureBase> FeaturePtr : Features)
	{
		const UMatchFeatureBase* Feature = FeaturePtr.Get();

		if(!Feature)
		{
			continue;
		}

		const int32 FeatureSize = Feature->Size();
		if(Feature->PoseCategory == EPoseCategory::Quality)
		{
			for(int32 i = 0; i < FeatureSize; ++i)
			{
				Weights[AtomIndex] = MotionMatchConfig->DefaultCalibrationArray[AtomIndex] * QualityMultiplier * StdDeviationNormalizers.Weights[AtomIndex];
				++AtomIndex;
			}
		}
		else
		{
			for(int32 i = 0; i < FeatureSize; ++i)
			{
				Weights[AtomIndex] = MotionMatchConfig->DefaultCalibrationArray[AtomIndex] * ResponseMultiplier * StdDeviationNormalizers.Weights[AtomIndex];
				++AtomIndex;
			}
		}
	}
}
