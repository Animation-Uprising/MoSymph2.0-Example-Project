//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "Objects/MatchFeatures/MatchFeatureBase.h"
#include "Enumerations/EMMPreProcessEnums.h"
#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"

#include "MotionMatchConfig.generated.h"

class USkeleton;

UCLASS(BlueprintType)
class MOTIONSYMPHONY_API UMotionMatchConfig : public UObject, public IBoneReferenceSkeletonProvider
{
	GENERATED_BODY()

public:
	UMotionMatchConfig(const FObjectInitializer& ObjectInitializer);
	
public:
	/** The source skeleton that this motion match configuration is based on and compatible with */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General")
	USkeleton* SourceSkeleton;

	//Todo: Make the axis' editor only data which gets removed for a full build
	/** The up axis of the model*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General")
	EAllAxis UpAxis;

	/** The forward axis of the model. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "General")
	EAllAxis ForwardAxis;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default Calibration", meta = (ClampMin = 0, ClampMax = 1))
	float DefaultQualityVsResponsivenessRatio = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Default Calibration", meta = (ClampMin = 0.0f))
	float ResultantVelocityWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Default Calibration")
	bool bNormalizeWeightsByQuantity = false;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Features")
	TArray<TObjectPtr<UMatchFeatureBase>> InputResponseFeatures;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Features")
	TArray<TObjectPtr<UMatchFeatureBase>> PoseQualityFeatures;

	/** A list of features that the end user would like to match */
	TArray<TObjectPtr<UMatchFeatureBase>> Features;
	
	/** Number of dimensions in the response portion of features*/ 
	UPROPERTY()
	int32 ResponseDimensionCount = 0;

	/** Number of dimensions in the quality portion of features*/
	UPROPERTY()
	int32 QualityDimensionCount = 0;
	
	/** Total number of dimensions (Atoms) for all features.*/
	UPROPERTY()
	int32 TotalDimensionCount = 0;
	
	UPROPERTY(Transient)
	TArray<float> DefaultCalibrationArray;

public:
	void Initialize();
	bool NeedsInitialization() const;

	virtual USkeleton* GetSkeleton(bool& bInvalidSkeletonIsError, const class IPropertyHandle* PropertyHandle) override;
	USkeleton* GetSourceSkeleton() const;
	void SetSourceSkeleton(USkeleton* Skeleton);
	
	bool IsSetupValidForMotionMatching();
	
	int32 ComputeResponseArraySize();
	int32 ComputeQualityArraySize();
};
