//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Objects/Assets/MotionMatchConfig.h"
#include "MotionCalibration.generated.h"

enum class EMotionCalibrationType : uint8;
class UMotionDataAsset;

UCLASS(Blueprintable, BlueprintType)
class MOTIONSYMPHONY_API UMotionCalibration : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	TObjectPtr<UMotionMatchConfig> MotionMatchConfig;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General") 
	EMotionCalibrationType CalibrationType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Calibration", meta = (ClampMin = 0, ClampMax = 1))
	float QualityVsResponsivenessRatio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Calibration")
	TArray<float> CalibrationArray;
	
	UPROPERTY(Transient)
	TArray<float> AdjustedCalibrationArray;

public:
	UMotionCalibration(const FObjectInitializer& ObjectInitializer);

	void Initialize();
	void Reset();
	void Clear();
	void ValidateData(TObjectPtr<UMotionMatchConfig> InMotionMatchConfig, const bool bForceConfigInitialization = true);
	bool IsSetupValid(UMotionMatchConfig* InMotionMatchConfig) const;

	virtual void Serialize(FArchive& Ar) override;

	UFUNCTION(BlueprintNativeEvent, Category = "MotionSymphony|CostFunctions")
	void OnGenerateWeightings();
	void OnGenerateWeightings_Implementation();


#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};