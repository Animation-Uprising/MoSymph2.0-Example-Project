//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Objects/MatchFeatures/MatchFeature_BoneFacing.h"
#include "MMPreProcessUtils.h"
#include "MotionAnimAsset.h"
#include "MotionDataAsset.h"
#include "Animation/AnimInstanceProxy.h"


#if WITH_EDITOR
#include "Animation/DebugSkelMeshComponent.h"
#include "MotionSymphonySettings.h"
#endif

bool UMatchFeature_BoneFacing::IsMotionSnapshotCompatible() const
{
	return true;
}

int32 UMatchFeature_BoneFacing::Size() const
{
	return 3;
}

void UMatchFeature_BoneFacing::EvaluatePreProcess(float* ResultLocation, UAnimSequence* InSequence,
                                                  const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, ::TObjectPtr<
                                                  UMotionAnimObject> InMotionObject)
{
	if(!InSequence)
	{
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		return;
	}
	
	FTransform JointTransform_CS = FTransform::Identity;
	TArray<FName> BonesToRoot;
	FName BoneName = BoneReference.BoneName;
	
	if(bMirror && MirrorDataTable)
	{
		BoneName = FMMPreProcessUtils::FindMirrorBoneName(InSequence->GetSkeleton(), MirrorDataTable, BoneName);
	}

	FMMPreProcessUtils::FindBonePathToRoot(InSequence, BoneName, BonesToRoot);
	if(BonesToRoot.Num() > 0)
	{
		BonesToRoot.RemoveAt(BonesToRoot.Num() - 1); //Removes the root
	}
	
	FMMPreProcessUtils::GetJointTransform_RootRelative(JointTransform_CS, InSequence, BonesToRoot, Time);
	const FVector BoneFacing = JointTransform_CS.GetUnitAxis(FacingAxis).GetSafeNormal() * 100.0f; // We do this to make sure the vector is 1m long
	
	*ResultLocation = bMirror? -BoneFacing.X : BoneFacing.X;
	++ResultLocation;
	*ResultLocation = BoneFacing.Y;
	++ResultLocation;
	*ResultLocation = BoneFacing.Z;
}

void UMatchFeature_BoneFacing::EvaluatePreProcess(float* ResultLocation, UAnimComposite* InComposite,
                                                  const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, TObjectPtr<
                                                  UMotionAnimObject> InAnimObject)
{
	if(!InComposite)
	{
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		return;
	}
	
	FTransform JointTransform_CS = FTransform::Identity;
	TArray<FName> BonesToRoot;
	FName BoneName = BoneReference.BoneName;
	
	if(bMirror && MirrorDataTable)
	{
		BoneName = FMMPreProcessUtils::FindMirrorBoneName(InComposite->GetSkeleton(), MirrorDataTable, BoneName);
	}

	FMMPreProcessUtils::FindBonePathToRoot(InComposite, BoneName, BonesToRoot);
	if(BonesToRoot.Num() > 0)
	{
		BonesToRoot.RemoveAt(BonesToRoot.Num() - 1); //Removes the root
	}
	
	FMMPreProcessUtils::GetJointTransform_RootRelative(JointTransform_CS, InComposite, BonesToRoot, Time);
	const FVector BoneFacing = JointTransform_CS.GetUnitAxis(FacingAxis).GetSafeNormal() * 100.0f;
	
	*ResultLocation = bMirror? -BoneFacing.X : BoneFacing.X;
	++ResultLocation;
	*ResultLocation = BoneFacing.Y;
	++ResultLocation;
	*ResultLocation = BoneFacing.Z;
}

void UMatchFeature_BoneFacing::EvaluatePreProcess(float* ResultLocation, UBlendSpace* InBlendSpace,
                                                  const float Time, const float PoseInterval, const bool bMirror,
                                                  UMirrorDataTable* MirrorDataTable, const FVector2D BlendSpacePosition, TObjectPtr<UMotionAnimObject> InAnimObject)
{
	if(!InBlendSpace)
	{
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		return;
	}

	TArray<FBlendSample> BlendSamples =  InBlendSpace->GetBlendSamples();
	TArray<FBlendSampleData> SampleDataList;
	int32 CachedTriangulationIndex = -1;
	InBlendSpace->GetSamplesFromBlendInput(FVector(BlendSpacePosition.X, BlendSpacePosition.Y, 0.0f),
		SampleDataList, CachedTriangulationIndex, false);
	
	FTransform JointTransform_CS = FTransform::Identity;
	TArray<FName> BonesToRoot;
	FName BoneName = BoneReference.BoneName;
	
	if(bMirror && MirrorDataTable)
	{
		BoneName = FMMPreProcessUtils::FindMirrorBoneName(InBlendSpace->GetSkeleton(), MirrorDataTable, BoneName);
	}

	FMMPreProcessUtils::FindBonePathToRoot(BlendSamples[0].Animation.Get(), BoneName, BonesToRoot);
	if(BonesToRoot.Num() > 0)
	{
		BonesToRoot.RemoveAt(BonesToRoot.Num() - 1); //Removes the root
	}
	
	FMMPreProcessUtils::GetJointTransform_RootRelative(JointTransform_CS, SampleDataList, BonesToRoot, Time);
	const FVector BoneFacing = JointTransform_CS.GetUnitAxis(FacingAxis).GetSafeNormal() * 100.0f;
	
	*ResultLocation = bMirror? -BoneFacing.X : BoneFacing.X;
	++ResultLocation;
	*ResultLocation = BoneFacing.Y;
	++ResultLocation;
	*ResultLocation = BoneFacing.Z;
}

void UMatchFeature_BoneFacing::CacheMotionBones(const FAnimInstanceProxy* InAnimInstanceProxy)
{
	BoneReference.Initialize(InAnimInstanceProxy->GetRequiredBones());
	//BoneReference.GetCompactPoseIndex(InAnimInstanceProxy->GetRequiredBones());
}

void UMatchFeature_BoneFacing::ExtractRuntime(FCSPose<FCompactPose>& CSPose, float* ResultLocation, float* FeatureCacheLocation, FAnimInstanceProxy*
                                              AnimInstanceProxy, float DeltaTime)
{
	const FVector BoneFacing = CSPose.GetComponentSpaceTransform(FCompactPoseBoneIndex(BoneReference.CachedCompactPoseIndex)).GetUnitAxis(FacingAxis) * 100.0f;

	*ResultLocation = BoneFacing.X;
	++ResultLocation;
	*ResultLocation = BoneFacing.Y;
	++ResultLocation;
	*ResultLocation = BoneFacing.Z;
}

void UMatchFeature_BoneFacing::CalculateDistanceSqrToMeanArrayForStandardDeviations(TArray<float>& OutDistToMeanSqrArray,
	const TArray<float>& InMeanArray, const TArray<float>& InPoseArray, const int32 FeatureOffset, const int32 PoseStartIndex) const
{
	const FVector Facing
	{
		InPoseArray[PoseStartIndex + FeatureOffset],
		InPoseArray[PoseStartIndex + FeatureOffset + 1],
		InPoseArray[PoseStartIndex + FeatureOffset + 2]
	};

	const FVector MeanFacing
	{
		InMeanArray[FeatureOffset],
		InMeanArray[FeatureOffset + 1],
		InMeanArray[FeatureOffset + 2]
	};

	const float DistanceToMean = FVector::DistSquared(Facing, MeanFacing);

	OutDistToMeanSqrArray[FeatureOffset] += DistanceToMean;
	OutDistToMeanSqrArray[FeatureOffset+1] += DistanceToMean;
	OutDistToMeanSqrArray[FeatureOffset+2] += DistanceToMean;
}

bool UMatchFeature_BoneFacing::CanBeQualityFeature() const
{
	return true;
}

#if WITH_EDITOR
void UMatchFeature_BoneFacing::DrawPoseDebugEditor(UMotionDataAsset* MotionData,
                                                     UDebugSkelMeshComponent* DebugSkeletalMesh, const int32 PreviewIndex, const int32 FeatureOffset,
                                                     const UWorld* World, FPrimitiveDrawInterface* DrawInterface)
{
	if(!MotionData || !DebugSkeletalMesh || !World)
	{
		return;
	}

	TArray<float>& PoseArray = MotionData->LookupPoseMatrix.PoseArray;
	const int32 StartIndex = PreviewIndex * MotionData->LookupPoseMatrix.AtomCount + FeatureOffset;
	
	if(PoseArray.Num() < StartIndex + Size())
	{
		return;
	}

	const UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();
	const FColor DebugColor = Settings->DebugColor_JointFacing;

	const FTransform PreviewTransform = DebugSkeletalMesh->GetComponentTransform();

	const FVector StartPoint = DebugSkeletalMesh->GetBoneLocation(BoneReference.BoneName, EBoneSpaces::WorldSpace);
	const FVector EndPoint = StartPoint + PreviewTransform.TransformVector(FVector(PoseArray[StartIndex],
		PoseArray[StartIndex+1], PoseArray[StartIndex+2]) / 3.0f);

	DrawDebugDirectionalArrow(World, StartPoint, EndPoint, 20.0f, DebugColor, true, -1, -1, 1.5f);
}

void UMatchFeature_BoneFacing::DrawDebugDesiredRuntime(FAnimInstanceProxy* AnimInstanceProxy,
	FMotionMatchingInputData& InputData, const int32 FeatureOffset, UMotionMatchConfig* MMConfig)
{
	
}

void UMatchFeature_BoneFacing::DrawDebugCurrentRuntime(FAnimInstanceProxy* AnimInstanceProxy,
                                                       TObjectPtr<const UMotionDataAsset> MotionData, const TArray<float>& CurrentPoseArray, const int32 FeatureOffset)
{
	if(!AnimInstanceProxy || !MotionData)
	{
		return;
	}

	const UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();
	const FColor DebugColor = Settings->DebugColor_JointFacing;

	const USkeletalMeshComponent* SkelMeshComponent = AnimInstanceProxy->GetSkelMeshComponent();
	const FTransform PreviewTransform = SkelMeshComponent->GetComponentTransform();
	
	const FVector StartPoint = SkelMeshComponent->GetBoneLocation(BoneReference.BoneName, EBoneSpaces::WorldSpace);
	const FVector EndPoint = StartPoint + PreviewTransform.TransformVector(FVector(CurrentPoseArray[FeatureOffset],
		CurrentPoseArray[FeatureOffset+1], CurrentPoseArray[FeatureOffset+2]) / 3.0f);
	
	AnimInstanceProxy->AnimDrawDebugDirectionalArrow(StartPoint, EndPoint, 40.0f,
		DebugColor, false, -1.0f, 2.0f, SDPG_Foreground);
}
#endif