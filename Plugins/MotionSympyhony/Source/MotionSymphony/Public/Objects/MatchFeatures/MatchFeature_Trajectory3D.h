//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MatchFeatureBase.h"
#include "MatchFeature_Trajectory3D.generated.h"


class UMotionDataAsset;
struct FPoseMatrix;

/** Motion matching feature which stores and matches bone velocity relative to the character root */
UCLASS(BlueprintType)
class MOTIONSYMPHONY_API UMatchFeature_Trajectory3D : public UMatchFeatureBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Match Feature")
	float PastTrajectoryWeightMultiplier;
	
	UPROPERTY(EditAnywhere, Category = "Match Feature")
	float DefaultDirectionWeighting;
	
	UPROPERTY(EditAnywhere, Category = "Match Feature|Trajectory")
	TArray<float> TrajectoryTiming;

public:
	UMatchFeature_Trajectory3D();
	virtual bool IsSetupValid() const override;
	virtual int32 Size() const override;
	virtual void EvaluatePreProcess(float* ResultLocation, UAnimSequence* InSequence,
	                                const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, ::TObjectPtr<
	                                UMotionAnimObject> InMotionObject) override;
	virtual void EvaluatePreProcess(float* ResultLocation, UAnimComposite* InComposite,
	                                const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, TObjectPtr<
	                                UMotionAnimObject> InAnimObject) override;
	virtual void EvaluatePreProcess(float* ResultLocation, UBlendSpace* InBlendSpace,
	                                const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, const FVector2D BlendSpacePosition, TObjectPtr
	                                <UMotionAnimObject>
	                                InAnimObject) override;
	//virtual void EvaluateRuntime(float* ResultLocation) override;

	virtual void SourceInputData(TArray<float>& OutFeatureArray, const int32 FeatureOffset, AActor* InActor) override;
	virtual void ApplyInputBlending(TArray<float>& DesiredInputArray, const TArray<float>& CurrentPoseArray, const int32 FeatureOffset, const float Weight) override;
	virtual bool NextPoseToleranceTest(const TArray<float>& DesiredInputArray, const TArray<float>& PoseMatrix,
	                                   const int32 MatrixStartIndex, const int32 FeatureOffset, const float PositionTolerance, const float RotationTolerance) override;

	virtual float GetDefaultWeight(int32 AtomId) const override;

	virtual void CalculateDistanceSqrToMeanArrayForStandardDeviations(TArray<float>& OutDistToMeanSqrArray,
	                                                                  const TArray<float>& InMeanArray, const TArray<float>& InPoseArray, const int32 FeatureOffset, const int32 PoseStartIndex) const override;
	
	virtual bool CanBeResponseFeature() const override;
	
#if WITH_EDITOR
	virtual void DrawPoseDebugEditor(UMotionDataAsset* MotionData, UDebugSkelMeshComponent* DebugSkeletalMesh,
		const int32 PreviewIndex, const int32 FeatureOffset, const UWorld* World, FPrimitiveDrawInterface* DrawInterface) override;
	virtual void DrawDebugDesiredRuntime(FAnimInstanceProxy* AnimInstanceProxy, FMotionMatchingInputData& InputData,
		const int32 FeatureOffset, UMotionMatchConfig* MMConfig) override;
	virtual void DrawDebugCurrentRuntime(FAnimInstanceProxy* AnimInstanceProxy, TObjectPtr<const UMotionDataAsset> MotionData,
	                                     const TArray<float>& CurrentPoseArray, const int32 FeatureOffset)  override;
#endif
};