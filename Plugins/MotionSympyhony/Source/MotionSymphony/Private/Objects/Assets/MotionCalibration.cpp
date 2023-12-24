//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Objects/Assets/MotionCalibration.h"

#define LOCTEXT_NAMESPACE "MotionCalibration"

UMotionCalibration::UMotionCalibration(const FObjectInitializer& ObjectInitializer)
	: UObject(ObjectInitializer),
	MotionMatchConfig(nullptr),
	QualityVsResponsivenessRatio(0.5f)
{}

void UMotionCalibration::Initialize()
{
	if (!MotionMatchConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("Trying to initialize MotionCalibration but the MotionMatchConfig is null. Please ensure your Motion Calibration has a configuration set."));
		return;
	}
	
	if(CalibrationArray.Num() != MotionMatchConfig->TotalDimensionCount)
	{
		OnGenerateWeightings();
	}

	AdjustedCalibrationArray = CalibrationArray;

	int32 AtomIndex = 0;
	const float QualityAdjustment = (1.0f - QualityVsResponsivenessRatio) * 2.0f;
	const float ResponsivenessAdjustment = QualityVsResponsivenessRatio * 2.0f;
	for(TObjectPtr<UMatchFeatureBase> MatchFeaturePtr : MotionMatchConfig->Features)
	{
		const UMatchFeatureBase* MatchFeature = MatchFeaturePtr.Get();
		
		if(MatchFeature->PoseCategory == EPoseCategory::Quality)
		{
			for(int32 i = 0; i < MatchFeature->Size(); ++i)
			{
				AdjustedCalibrationArray[AtomIndex] *= QualityAdjustment;
				++AtomIndex;
			}
		}
		else
		{
			for(int32 i = 0; i < MatchFeature->Size(); ++i)
			{
				AdjustedCalibrationArray[AtomIndex] *= ResponsivenessAdjustment;
				++AtomIndex;
			}
		}
	}

	Modify(); 
}

void UMotionCalibration::Reset()
{
	QualityVsResponsivenessRatio = 0.5f;

	for(int32 i = 0; i < CalibrationArray.Num(); ++i)
	{
		CalibrationArray[i] = 1.0f;
	}
}

void UMotionCalibration::Clear()
{
	QualityVsResponsivenessRatio = 0.5f;
	CalibrationArray.Empty(CalibrationArray.GetSlack());
}

void UMotionCalibration::ValidateData(TObjectPtr<UMotionMatchConfig> InMotionMatchConfig, const bool bForceConfigInitialization /*=true*/)
{
	if (!MotionMatchConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("Motion Calibration validation failed. Motion matching config not set"));
		return;
	}

	if(MotionMatchConfig != InMotionMatchConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("Motion Calibration validation failed. Motion Match config in the calibration asset and in the node must matched or else they cannot be used together."));
		return;
	}

	//Ensure that the calibration array is the correct size
	if(bForceConfigInitialization)
	{
		MotionMatchConfig->Initialize();
	}
	
	if(CalibrationArray.Num() != MotionMatchConfig->TotalDimensionCount
		|| AdjustedCalibrationArray.Num() != MotionMatchConfig->TotalDimensionCount)
	{
		UE_LOG(LogTemp, Warning, TEXT("Motion Calibration was not compliant with Motion Match Config. Calibration has been initialized to avoid a crash. Please initialize calibration data and review weightings."));
		Initialize();
	}
}

bool UMotionCalibration::IsSetupValid(UMotionMatchConfig* InMotionMatchConfig) const
{
	if (!MotionMatchConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("Motion Calibration validity check failed. The MotionMatchConfig property has not been set."));
		return false;
	}

	if (MotionMatchConfig != InMotionMatchConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("Motion Calibration validity check failed. The MotionMatchConfig property does not match the config set on the MotionData Asset."));
		return false;
	}

	if(CalibrationArray.Num() != InMotionMatchConfig->TotalDimensionCount)
	{
		UE_LOG(LogTemp, Error, TEXT("Motion Calibration validity check failed. The calibration array does not match the motion match config provided. Either the calibration data has not been initialized or the motion match config has not been processed."));
		return false;
	}

	return true;
}

void UMotionCalibration::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

}

void UMotionCalibration::OnGenerateWeightings_Implementation()
{
	if(MotionMatchConfig->NeedsInitialization())
	{
		MotionMatchConfig->Initialize();
	}

	const int32 InitialCalibrationSize = CalibrationArray.Num();
	CalibrationArray.SetNum(MotionMatchConfig->TotalDimensionCount);
	for(int32 i = InitialCalibrationSize; i < MotionMatchConfig->TotalDimensionCount; ++i)
	{
		CalibrationArray[i] = 1.0f;
	}
}

#if WITH_EDITORONLY_DATA
void UMotionCalibration::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);

	ValidateData(MotionMatchConfig);
}
#endif

#undef LOCTEXT_NAMESPACE