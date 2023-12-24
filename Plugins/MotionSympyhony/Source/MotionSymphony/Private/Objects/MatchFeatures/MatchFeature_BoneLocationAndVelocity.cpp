//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Objects/MatchFeatures/MatchFeature_BoneLocationAndVelocity.h"
#include "MMPreProcessUtils.h"
#include "MotionAnimAsset.h"
#include "MotionAnimObject.h"
#include "MotionDataAsset.h"
#include "Animation/AnimInstanceProxy.h"

#if WITH_EDITOR
#include "Animation/DebugSkelMeshComponent.h"
#include "MotionSymphonySettings.h"
#endif

UMatchFeature_BoneLocationAndVelocity::UMatchFeature_BoneLocationAndVelocity()
		: DefaultVelocityWeighting(1.0f)
{
}

bool UMatchFeature_BoneLocationAndVelocity::IsMotionSnapshotCompatible() const
{
	return true;
}

int32 UMatchFeature_BoneLocationAndVelocity::Size() const
{
	return 6;
}

void UMatchFeature_BoneLocationAndVelocity::EvaluatePreProcess(float* ResultLocation, UAnimSequence* InSequence,
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
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		return;
	}
	
	const float StartTime = FMath::Max(Time - (PoseInterval / 2.0f), 0.0f);

	FTransform JointTransform_CS =  FTransform::Identity;
	FTransform BeforeTransform_CS = FTransform::Identity;
	FTransform AfterTransform_CS = FTransform::Identity;
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
	FMMPreProcessUtils::GetJointTransform_RootRelative(BeforeTransform_CS, InSequence, BonesToRoot, StartTime);
	FMMPreProcessUtils::GetJointTransform_RootRelative(AfterTransform_CS, InSequence, BonesToRoot, StartTime + PoseInterval);

	const FVector BoneLocation = JointTransform_CS.GetLocation();

	//Extract User Data
	float PlayRate = 1.0f;
	if(InMotionObject)
	{
		PlayRate = InMotionObject->GetPlayRate();
	}
	
	const FVector BoneVelocity = ((AfterTransform_CS.GetLocation() - BeforeTransform_CS.GetLocation()) * PlayRate) / PoseInterval;

	*ResultLocation = bMirror? -BoneLocation.X : BoneLocation.X;
	++ResultLocation;
	*ResultLocation = BoneLocation.Y;
	++ResultLocation;
	*ResultLocation = BoneLocation.Z;
	++ResultLocation;
	*ResultLocation = bMirror? -BoneVelocity.X : BoneVelocity.X;
	++ResultLocation;
	*ResultLocation = BoneVelocity.Y;
	++ResultLocation;
	*ResultLocation = BoneVelocity.Z;
}

void UMatchFeature_BoneLocationAndVelocity::EvaluatePreProcess(float* ResultLocation, UAnimComposite* InComposite,
                                                               const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, TObjectPtr<
                                                               UMotionAnimObject> InMotionObject)
{
	if(!InComposite)
	{
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		return;
	}

	const float StartTime = FMath::Min(Time - (PoseInterval / 2.0f), 0.0f);

	FTransform JointTransform_CS =  FTransform::Identity;
	FTransform BeforeTransform_CS = FTransform::Identity;
	FTransform AfterTransform_CS = FTransform::Identity;
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
	FMMPreProcessUtils::GetJointTransform_RootRelative(BeforeTransform_CS, InComposite, BonesToRoot, StartTime);
	FMMPreProcessUtils::GetJointTransform_RootRelative(AfterTransform_CS, InComposite, BonesToRoot, StartTime + PoseInterval);

	const FVector BoneLocation = JointTransform_CS.GetLocation();

	//Extract User Data
	float PlayRate = 1.0f;
	if(InMotionObject)
	{
		PlayRate = InMotionObject->GetPlayRate();
	}
	
	const FVector BoneVelocity = ((AfterTransform_CS.GetLocation() - BeforeTransform_CS.GetLocation()) * PlayRate) / PoseInterval;
	
	*ResultLocation = bMirror? -BoneLocation.X : BoneLocation.X;
	++ResultLocation;
	*ResultLocation = BoneLocation.Y;
	++ResultLocation;
	*ResultLocation = BoneLocation.Z;
	++ResultLocation;
	*ResultLocation = bMirror? -BoneVelocity.X : BoneVelocity.X;
	++ResultLocation;
	*ResultLocation = BoneVelocity.Y;
	++ResultLocation;
	*ResultLocation = BoneVelocity.Z;
}

void UMatchFeature_BoneLocationAndVelocity::EvaluatePreProcess(float* ResultLocation, UBlendSpace* InBlendSpace,
                                                               const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, const FVector2D BlendSpacePosition, TObjectPtr
                                                               <UMotionAnimObject> InMotionObject)
{
	if(!InBlendSpace)
	{
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		return;
	}
	
	const float StartTime = FMath::Min(Time - (PoseInterval / 2.0f), 0.0f);

	FTransform JointTransform_CS = FTransform::Identity;
	FTransform BeforeTransform_CS = FTransform::Identity;
	FTransform AfterTransform_CS = FTransform::Identity;
	TArray<FName> BonesToRoot;
	FName BoneName = BoneReference.BoneName;

	TArray<FBlendSampleData> SampleDataList;
	int32 CachedTriangulationIndex = -1;
	InBlendSpace->GetSamplesFromBlendInput(FVector(BlendSpacePosition.X, BlendSpacePosition.Y, 0.0f),
		SampleDataList, CachedTriangulationIndex, false);

	if(bMirror && MirrorDataTable)
	{
		BoneName = BoneName = FMMPreProcessUtils::FindMirrorBoneName(InBlendSpace->GetSkeleton(), MirrorDataTable, BoneName);
	}

	FMMPreProcessUtils::FindBonePathToRoot(InBlendSpace->GetBlendSamples()[0].Animation.Get(), BoneName, BonesToRoot);
	if(BonesToRoot.Num() > 0)
	{
		BonesToRoot.RemoveAt(BonesToRoot.Num() - 1); //Removes the root
	}

	FMMPreProcessUtils::GetJointTransform_RootRelative(JointTransform_CS, SampleDataList, BonesToRoot, Time);
	FMMPreProcessUtils::GetJointTransform_RootRelative(BeforeTransform_CS, SampleDataList, BonesToRoot, StartTime);
	FMMPreProcessUtils::GetJointTransform_RootRelative(AfterTransform_CS, SampleDataList, BonesToRoot, StartTime + PoseInterval);

	const FVector BoneLocation = JointTransform_CS.GetLocation();

	//Extract User Data
	float PlayRate = 1.0f;
	if(InMotionObject)
	{
		PlayRate = InMotionObject->GetPlayRate();
	}
	
	const FVector BoneVelocity = ((AfterTransform_CS.GetLocation() - BeforeTransform_CS.GetLocation()) * PlayRate) / PoseInterval;
	
	*ResultLocation = bMirror? -BoneLocation.X : BoneLocation.X;
	++ResultLocation;
	*ResultLocation = BoneLocation.Y;
	++ResultLocation;
	*ResultLocation = BoneLocation.Z;
	++ResultLocation;
	*ResultLocation = bMirror? -BoneVelocity.X : BoneVelocity.X;
	++ResultLocation;
	*ResultLocation = BoneVelocity.Y;
	++ResultLocation;
	*ResultLocation = BoneVelocity.Z;
}

void UMatchFeature_BoneLocationAndVelocity::CacheMotionBones(const FAnimInstanceProxy* InAnimInstanceProxy)
{
	BoneReference.Initialize(InAnimInstanceProxy->GetRequiredBones());
	//BoneReference.GetCompactPoseIndex(InAnimInstanceProxy->GetRequiredBones());
}

void UMatchFeature_BoneLocationAndVelocity::ExtractRuntime(FCSPose<FCompactPose>& CSPose, float* ResultLocation, float* FeatureCacheLocation, FAnimInstanceProxy*
                                                AnimInstanceProxy, float DeltaTime)
{
	const FVector BoneLocation = CSPose.GetComponentSpaceTransform(BoneReference.CachedCompactPoseIndex).GetLocation();
	const FVector LastBoneLocation(*FeatureCacheLocation , *(FeatureCacheLocation+1), *(FeatureCacheLocation + 2));
	const FVector Velocity = (BoneLocation - LastBoneLocation) / FMath::Max(0.00001f, DeltaTime);

	//Save the Bone Location for velocity calculation next frame
	*FeatureCacheLocation = BoneLocation.X;
	++FeatureCacheLocation;
	*FeatureCacheLocation = BoneLocation.Y;
	++FeatureCacheLocation;
	*FeatureCacheLocation = BoneLocation.Z;
	
	//Save the velocity to the result location
	*ResultLocation = BoneLocation.X;
	++ResultLocation;
	*ResultLocation = BoneLocation.Y;
	++ResultLocation;
	*ResultLocation = BoneLocation.Z;
	++ResultLocation;
	*ResultLocation = Velocity.X;
	++ResultLocation;
	*ResultLocation = Velocity.Y;
	++ResultLocation;
	*ResultLocation = Velocity.Z;
}

float UMatchFeature_BoneLocationAndVelocity::GetDefaultWeight(int32 AtomId) const
{
	if(AtomId > 2)
	{
		return DefaultVelocityWeighting;
	}

	return DefaultWeight;
}

void UMatchFeature_BoneLocationAndVelocity::CalculateDistanceSqrToMeanArrayForStandardDeviations(TArray<float>& OutDistToMeanSqrArray,
                                                                                                 const TArray<float>& InMeanArray, const TArray<float>& InPoseArray, const int32 FeatureOffset, const int32 PoseStartIndex) const
{
	const FVector Location
	{
		InPoseArray[PoseStartIndex + FeatureOffset],
		InPoseArray[PoseStartIndex + FeatureOffset + 1],
		InPoseArray[PoseStartIndex + FeatureOffset + 2]
	};

	const FVector Velocity
	{
		InPoseArray[PoseStartIndex + FeatureOffset + 3],
		InPoseArray[PoseStartIndex + FeatureOffset + 4],
		InPoseArray[PoseStartIndex + FeatureOffset + 5]
	};

	const FVector MeanLocation
	{
		InMeanArray[FeatureOffset],
		InMeanArray[FeatureOffset + 1],
		InMeanArray[FeatureOffset + 2]
	};
	
	const FVector MeanVelocity
	{
		InMeanArray[FeatureOffset + 3],
		InMeanArray[FeatureOffset + 4],
		InMeanArray[FeatureOffset + 5]
	};
	
	const float DistanceToMean_Location = FVector::DistSquared(Location, MeanLocation);
	const float DistanceToMean_Velocity = FVector::DistSquared(Velocity, MeanVelocity);

	OutDistToMeanSqrArray[FeatureOffset] += DistanceToMean_Location;
	OutDistToMeanSqrArray[FeatureOffset+1] += DistanceToMean_Location;
	OutDistToMeanSqrArray[FeatureOffset+2] += DistanceToMean_Location;
	OutDistToMeanSqrArray[FeatureOffset+3] += DistanceToMean_Velocity;
	OutDistToMeanSqrArray[FeatureOffset+4] += DistanceToMean_Velocity;
	OutDistToMeanSqrArray[FeatureOffset+5] += DistanceToMean_Velocity;
}

bool UMatchFeature_BoneLocationAndVelocity::CanBeQualityFeature() const
{
	return true;
}

#if WITH_EDITOR
void UMatchFeature_BoneLocationAndVelocity::DrawPoseDebugEditor(UMotionDataAsset* MotionData,
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
	const FColor DebugColor = Settings->DebugColor_Joint;

	const FTransform PreviewTransform = DebugSkeletalMesh->GetComponentTransform();
	
	const FVector BonePos = PreviewTransform.TransformPosition(FVector(PoseArray[StartIndex],
		PoseArray[StartIndex+1], PoseArray[StartIndex+2]));
	const FVector EndPoint = BonePos + PreviewTransform.TransformVector(FVector(PoseArray[StartIndex+3],
		PoseArray[StartIndex+4], PoseArray[StartIndex+5]) * 0.333f);

	DrawDebugDirectionalArrow(World, BonePos, EndPoint, 20.0f, DebugColor, true,
		-1, -1, 1.5f);
	DrawDebugSphere(World, BonePos, 8.0f, 8, DebugColor, true, -1, -1);
}

void UMatchFeature_BoneLocationAndVelocity::DrawDebugDesiredRuntime(FAnimInstanceProxy* AnimInstanceProxy,
	FMotionMatchingInputData& InputData, const int32 FeatureOffset, UMotionMatchConfig* MMConfig)
{
	
}

void UMatchFeature_BoneLocationAndVelocity::DrawDebugCurrentRuntime(FAnimInstanceProxy* AnimInstanceProxy,
                                                                    TObjectPtr<const UMotionDataAsset> MotionData, const TArray<float>& CurrentPoseArray, const int32 FeatureOffset)
{
	if(!AnimInstanceProxy || !MotionData)
	{
		return;
	}

	const UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();
	const FColor DebugColor = Settings->DebugColor_Joint;

	const FTransform PreviewTransform = AnimInstanceProxy->GetSkelMeshComponent()->GetComponentTransform();

	const FVector BonePos = PreviewTransform.TransformPosition(FVector(CurrentPoseArray[FeatureOffset],
		CurrentPoseArray[FeatureOffset+1], CurrentPoseArray[FeatureOffset+2]));
	
	const FVector EndPoint = BonePos + PreviewTransform.TransformVector(FVector(CurrentPoseArray[FeatureOffset+3],
		CurrentPoseArray[FeatureOffset+4], CurrentPoseArray[FeatureOffset+5]) * 0.333f);

	AnimInstanceProxy->AnimDrawDebugSphere(BonePos, 8.0f, 8, DebugColor, false, -1,
		0, SDPG_Foreground);
	AnimInstanceProxy->AnimDrawDebugDirectionalArrow(BonePos, EndPoint, 40.0f,
		DebugColor, false, -1.0f, 2.0f, SDPG_Foreground);
}
#endif
