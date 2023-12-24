//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Objects/MatchFeatures/MatchFeatureBase.h"
#include "MMPreProcessUtils.h"

UMatchFeatureBase::UMatchFeatureBase()
	: DefaultWeight(1.0f)
{
}

void UMatchFeatureBase::Initialize()
{

}

bool UMatchFeatureBase::IsSetupValid() const
{
	return true;
}

bool UMatchFeatureBase::IsMotionSnapshotCompatible() const
{
	return false;
}

int32 UMatchFeatureBase::Size() const
{
	return 0;
}

void UMatchFeatureBase::EvaluatePreProcess(float* ResultLocation, UAnimSequence* InSequence, const float Time,
                                           const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, TObjectPtr<UMotionAnimObject>
                                           InMotionObject)
{
}

void UMatchFeatureBase::EvaluatePreProcess(float* ResultLocation, UAnimComposite* InComposite, const float Time,
                                           const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, TObjectPtr<UMotionAnimObject>
                                           InAnimObject)
{
}

void UMatchFeatureBase::EvaluatePreProcess(float* ResultLocation, UBlendSpace* InBlendSpace, const float Time, const float PoseInterval,
                                           const bool bMirror, UMirrorDataTable* MirrorDataTable, const FVector2D BlendSpacePosition, TObjectPtr<UMotionAnimObject
                                           > InAnimObject)
{
}

void UMatchFeatureBase::CleanupPreProcessData()
{
}

void UMatchFeatureBase::CacheMotionBones(const FAnimInstanceProxy* InAnimInstanceProxy)
{
}

void UMatchFeatureBase::ExtractRuntime(FCSPose<FCompactPose>& CSPose, float* ResultLocation, float* FeatureCacheLocation, FAnimInstanceProxy*
                                       AnimInstanceProxy, float DeltaTime)
{
}

void UMatchFeatureBase::SourceInputData(TArray<float>& OutFeatureArray, const int32 FeatureOffset, AActor* InActor)
{
	const int32 MaxIterations = FMath::Min(Size(), OutFeatureArray.Num() - FeatureOffset);
	for(int32 i = 0; i < MaxIterations; ++i)
	{
		OutFeatureArray[FeatureOffset + i] = 0.0f;
	}
}

void UMatchFeatureBase::ApplyInputBlending(TArray<float>& DesiredInputArray,
                                           const TArray<float>& CurrentPoseArray, const int32 FeatureOffset, const float Weight)
{
}

bool UMatchFeatureBase::NextPoseToleranceTest(const TArray<float>& DesiredInputArray, const TArray<float>& PoseMatrix,
                                              const int32 MatrixStartIndex, const int32 FeatureOffset, const float PositionTolerance, const float RotationTolerance)
{
	return true;
}

float UMatchFeatureBase::GetDefaultWeight(int32 AtomId) const
{
	return DefaultWeight;
}

void UMatchFeatureBase::CalculateDistanceSqrToMeanArrayForStandardDeviations(TArray<float>& OutDistToMeanSqrArray,
	const TArray<float>& InMeanArray, const TArray<float>& InPoseArray, const int32 FeatureOffset, const int32 PoseStartIndex) const
{
	for(int32 i = FeatureOffset; i < FeatureOffset + Size(); ++i)
	{
		const float DistanceToMean = InPoseArray[PoseStartIndex + i] - InMeanArray[i];
		OutDistToMeanSqrArray[i] += DistanceToMean * DistanceToMean;
	}
}

bool UMatchFeatureBase::CanBeQualityFeature() const
{
	return false;
}

bool UMatchFeatureBase::CanBeResponseFeature() const
{
	return false;
}

#if WITH_EDITOR
void UMatchFeatureBase::DrawPoseDebugEditor(UMotionDataAsset* MotionData, UDebugSkelMeshComponent* DebugSkeletalMesh,
                                            const int32 PreviewIndex, const int32 FeatureOffset, const UWorld* World, FPrimitiveDrawInterface* DrawInterface)
{
}

void UMatchFeatureBase::DrawAnimDebugEditor()
{
}

void UMatchFeatureBase::DrawDebugDesiredRuntime(FAnimInstanceProxy* AnimInstanceProxy, FMotionMatchingInputData&,
	const int32 FeatureOffset, UMotionMatchConfig* MMConfig)
{
}

void UMatchFeatureBase::DrawDebugCurrentRuntime(FAnimInstanceProxy* AnimInstanceProxy, TObjectPtr<const UMotionDataAsset> MotionData,
                                                const TArray<float>& CurrentPoseArray, const int32 FeatureOffset)
{
}

bool UMatchFeatureBase::operator<(const UMatchFeatureBase& other)
{
	if(PoseCategory == EPoseCategory::Responsiveness)
	{
		if(other.PoseCategory == EPoseCategory::Quality)
		{
			return true;
		}
	}

	return false;
}



#endif
