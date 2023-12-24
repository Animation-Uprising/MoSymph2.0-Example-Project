//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Objects/Assets/MotionMatchConfig.h"

#define LOCTEXT_NAMESPACE "MotionMatchConfig"

UMotionMatchConfig::UMotionMatchConfig(const FObjectInitializer& ObjectInitializer)
	: SourceSkeleton(nullptr),
	UpAxis(EAllAxis::Z),
	ForwardAxis(EAllAxis::Y)
{
}

void UMotionMatchConfig::Initialize()
{
	if (!SourceSkeleton)
	{
		UE_LOG(LogTemp, Error, TEXT("MotionMatchConfig: Trying to initialize bone references but there is no source skeleton set. Please set a skeleton on your motion match configuration before using it"));
	}

	TotalDimensionCount = 0;
	ResponseDimensionCount = 0;
	QualityDimensionCount = 0;
	
	DefaultCalibrationArray.Empty(InputResponseFeatures.Num() * 5 + PoseQualityFeatures.Num() * 3);
	Features.Empty(InputResponseFeatures.Num() + PoseQualityFeatures.Num());
	for(TObjectPtr<UMatchFeatureBase> MatchFeature : InputResponseFeatures)
	{
		if(MatchFeature && MatchFeature->IsSetupValid())
		{
			if(!MatchFeature->CanBeResponseFeature())
			{
				UE_LOG(LogTemp, Error, TEXT("MotionMatchConfig: You are using a match feature is the response feature list, but it does not support response input. Please check that the match feature is in the correct list. This may cause undefined results in motion matching"));
			}
			
			MatchFeature->PoseCategory = EPoseCategory::Responsiveness;
			MatchFeature->Initialize();
			Features.Add(MatchFeature);
			
			const int32 FeatureSize = MatchFeature->Size();
			ResponseDimensionCount += FeatureSize;
			
			for(int32 i = 0; i < FeatureSize; ++i)
			{
				DefaultCalibrationArray.Add(MatchFeature->GetDefaultWeight(i));
			}
		}
	}

	for(TObjectPtr<UMatchFeatureBase> MatchFeature : PoseQualityFeatures)
	{
		if(MatchFeature && MatchFeature->IsSetupValid())
		{
			if(!MatchFeature->CanBeQualityFeature())
			{
				UE_LOG(LogTemp, Error, TEXT("MotionMatchConfig: You are using a match feature is the quality feature list, but it does not support quality. Please check that the match feature is in the correct list. This may cause undefined results in motion matching"));
			}
			
			MatchFeature->PoseCategory = EPoseCategory::Quality;
			MatchFeature->Initialize();
			Features.Add(MatchFeature);

			const int32 FeatureSize = MatchFeature->Size();
			QualityDimensionCount += FeatureSize;

			for(int32 i = 0; i < FeatureSize; ++i)
			{
				DefaultCalibrationArray.Add(MatchFeature->GetDefaultWeight(i));
			}
		}
	}

	TotalDimensionCount = ResponseDimensionCount + QualityDimensionCount;

	//Second pass to apply quantity normalization
	if(bNormalizeWeightsByQuantity)
	{
		const float ResponseSizeNormalMultiplier = 1.0f - (static_cast<float>(ResponseDimensionCount) / static_cast<float>(TotalDimensionCount));
		const float QualitySizeNormalMultiplier = 1.0f - (static_cast<float>(QualityDimensionCount) / static_cast<float>(TotalDimensionCount));

		for(int32 i = 0; i < DefaultCalibrationArray.Num(); ++i)
		{
			if(i < ResponseDimensionCount)
			{
				DefaultCalibrationArray[i] *= ResponseSizeNormalMultiplier;
			}
			else
			{
				DefaultCalibrationArray[i] *= QualitySizeNormalMultiplier;
			}
		}
	}
}

bool UMotionMatchConfig::NeedsInitialization() const
{
	return DefaultCalibrationArray.Num() == 0
		|| DefaultCalibrationArray.Num() != TotalDimensionCount;
}

USkeleton* UMotionMatchConfig::GetSkeleton(bool& bInvalidSkeletonIsError, const class IPropertyHandle* PropertyHandle)
{
	return SourceSkeleton;
}

USkeleton* UMotionMatchConfig::GetSourceSkeleton() const
{
	return SourceSkeleton;
}

void UMotionMatchConfig::SetSourceSkeleton(USkeleton* Skeleton)
{
	SourceSkeleton = Skeleton;
}

bool UMotionMatchConfig::IsSetupValidForMotionMatching()
{
	bool bIsValid = true;

	Initialize();

	//Check that a source skeleton is set
	if (!SourceSkeleton)
	{
		UE_LOG(LogTemp, Error, TEXT("Motion Match Config: validity check failed. Source skeleton is not set (null)."));
		bIsValid = false;
	}

	int32 QualityFeatureCount = 0;
	int32 ResponseFeatureCount = 0;
	for(const UMatchFeatureBase* Feature : Features)
	{
		if(Feature)
		{
			if(Feature->PoseCategory == EPoseCategory::Quality)
			{
				++QualityFeatureCount;
			}
			else
			{
				++ResponseFeatureCount;
			}
			
			if(!Feature->IsSetupValid())
			{
				UE_LOG(LogTemp, Error, TEXT("Match Match Config: Validity check failed. One of the match features is not valid"));
				bIsValid = false;
				break;
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Motion Match Config: Validity check failed. There is an invalid (null) match feature present."));
			bIsValid = false;
			break;
		}
	}

	//Check that there is at least one quality match feature present
	if(QualityFeatureCount == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Motion Match Config: Valididty Check failed. There must be at least one quality feature in the motion config feature set."));
		bIsValid = false;
	}

	//Check that there is at least one response match feature present
	if(ResponseFeatureCount == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Motion Match Config: Validity check failed. THere must be at least one response feature in the motion config feature set."));
		bIsValid = false;
	}

	return bIsValid;
}

int32 UMotionMatchConfig::ComputeResponseArraySize()
{
	int32 Count = 0;
	for(const TObjectPtr<UMatchFeatureBase> MatchFeaturePtr : Features)
	{
		if(MatchFeaturePtr->PoseCategory == EPoseCategory::Responsiveness)
		{
			Count += MatchFeaturePtr->Size();
		}
	}

	return Count;
}

int32 UMotionMatchConfig::ComputeQualityArraySize()
{
	int32 Count = 0;
	for(const TObjectPtr<UMatchFeatureBase> MatchFeaturePtr : Features)
	{
		if(MatchFeaturePtr->PoseCategory == EPoseCategory::Quality)
		{
			Count += MatchFeaturePtr->Size();
		}
	}

	return Count;
}

#undef LOCTEXT_NAMESPACE
