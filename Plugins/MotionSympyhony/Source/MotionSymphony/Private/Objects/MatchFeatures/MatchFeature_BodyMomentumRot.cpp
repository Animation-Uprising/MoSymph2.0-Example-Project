//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Objects/MatchFeatures/MatchFeature_BodyMomentumRot.h"
#include "MMPreProcessUtils.h"
#include "MotionAnimAsset.h"
#include "MotionAnimObject.h"
#include "MotionDataAsset.h"
#include "Animation/AnimInstanceProxy.h"
#include "GameFramework/CharacterMovementComponent.h"

#if WITH_EDITOR
#include "Animation/DebugSkelMeshComponent.h"
#include "MotionSymphonySettings.h"
#endif

int32 UMatchFeature_BodyMomentumRot::Size() const
{
	return 1;
}

void UMatchFeature_BodyMomentumRot::EvaluatePreProcess(float* ResultLocation, UAnimSequence* InSequence,
                                                       const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, ::TObjectPtr<
                                                       UMotionAnimObject> InMotionObject)
{
	if(!InSequence)
	{
		*ResultLocation = 0.0f;
		return;
	}
	
	FVector RootVelocity;
	float RootRotVelocity;
	FMMPreProcessUtils::ExtractRootVelocity(RootVelocity, RootRotVelocity, InSequence, Time, PoseInterval);

	//Extract User Data
	if(InMotionObject)
	{
		RootVelocity *= InMotionObject->GetPlayRate();
	}
	
	*ResultLocation = bMirror ? -RootRotVelocity : RootRotVelocity;
}

void UMatchFeature_BodyMomentumRot::EvaluatePreProcess(float* ResultLocation, UAnimComposite* InComposite,
                                                       const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, TObjectPtr<
                                                       UMotionAnimObject> InMotionObject)
{
	if(!InComposite)
	{
		*ResultLocation = 0.0f;
		return;
	}
	
	FVector RootVelocity;
	float RootRotVelocity;
	FMMPreProcessUtils::ExtractRootVelocity(RootVelocity, RootRotVelocity, InComposite, Time, PoseInterval);


	//Extract User Data
	if(InMotionObject)
	{
		RootVelocity *= InMotionObject->GetPlayRate();
	}
	
	*ResultLocation = bMirror ? -RootRotVelocity : RootRotVelocity;
}

void UMatchFeature_BodyMomentumRot::EvaluatePreProcess(float* ResultLocation, UBlendSpace* InBlendSpace, const float Time,
                                                       const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, const FVector2D BlendSpacePosition, TObjectPtr
                                                       <UMotionAnimObject> InMotionObject)
{
	if(!InBlendSpace)
	{
		*ResultLocation = 0.0f;
		return;
	}

	TArray<FBlendSampleData> SampleDataList;
	int32 CachedTriangulationIndex = -1;
	InBlendSpace->GetSamplesFromBlendInput(FVector(BlendSpacePosition.X, BlendSpacePosition.Y, 0.0f),
		SampleDataList, CachedTriangulationIndex, false);

	
	 FVector RootVelocity;
	 float RootRotVelocity;
	 FMMPreProcessUtils::ExtractRootVelocity(RootVelocity, RootRotVelocity, SampleDataList, Time, PoseInterval);

	//Extract User Data
	if(InMotionObject)
	{
		RootVelocity *= InMotionObject->GetPlayRate();
	}
	
	*ResultLocation = bMirror ? -RootRotVelocity : RootRotVelocity;
}

void UMatchFeature_BodyMomentumRot::ExtractRuntime(FCSPose<FCompactPose>& CSPose, float* ResultLocation,
                                                   float* FeatureCacheLocation, FAnimInstanceProxy* AnimInstanceProxy, float DeltaTime)
{
	if(!AnimInstanceProxy)
	{
		*ResultLocation = 0.0f;
		*FeatureCacheLocation = 0.0f;
		return;
	}

	const float BodyRotation = AnimInstanceProxy->GetComponentTransform().GetRotation().Z;
	const float LastBodyRotation = *FeatureCacheLocation;
	const float RotationVelocity = (BodyRotation - LastBodyRotation) / FMath::Max(0.0000f, DeltaTime);
	
	*ResultLocation = RotationVelocity;
	*FeatureCacheLocation = BodyRotation;
}

bool UMatchFeature_BodyMomentumRot::CanBeQualityFeature() const
{
	return true;
}

bool UMatchFeature_BodyMomentumRot::CanBeResponseFeature() const
{
	return true;
}

#if WITH_EDITOR
void UMatchFeature_BodyMomentumRot::DrawPoseDebugEditor(UMotionDataAsset* MotionData,
                                                        UDebugSkelMeshComponent* DebugSkeletalMesh, const int32 PreviewIndex, const int32 FeatureOffset,
                                                        const UWorld* World, FPrimitiveDrawInterface* DrawInterface)
{
	if(!MotionData || !DebugSkeletalMesh ||!World)
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
	const FColor DebugColor = Settings->DebugColor_BodyAngularVelocity;

	const FTransform PreviewTransform = DebugSkeletalMesh->GetComponentTransform();
	const FVector RotatePoint = PreviewTransform.GetLocation();
	const FVector StartPoint = RotatePoint + PreviewTransform.TransformVector(FVector(50.0f, 0.0f, 0.0f));
	const FVector EndPoint = StartPoint + PreviewTransform.TransformVector(FVector(0.0f,PoseArray[StartIndex], 0.0f));

	DrawDebugCircle(World, RotatePoint, 50.0f, 32, DebugColor, true, -1, -1,
		1.5, FVector::LeftVector, FVector::ForwardVector, false);
	DrawDebugDirectionalArrow(World, StartPoint, EndPoint, 60.0f, DebugColor, true, -1, -1, 1.5f);
}

void UMatchFeature_BodyMomentumRot::DrawDebugCurrentRuntime(FAnimInstanceProxy* AnimInstanceProxy,
                                                            TObjectPtr<const UMotionDataAsset> MotionData, const TArray<float>& CurrentPoseArray, const int32 FeatureOffset)
{
	if(!AnimInstanceProxy || !MotionData)
	{
		return;
	}

	const UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();
	const FColor DebugColor = Settings->DebugColor_BodyAngularVelocity;

	const FTransform& MeshTransform = AnimInstanceProxy->GetSkelMeshComponent()->GetComponentTransform();
	const FVector MeshLocation = MeshTransform.GetLocation();
	const FVector StartPoint = MeshLocation + MeshTransform.TransformVector(FVector(50.0f, 0.0f, 0.0f));
	const FVector EndPoint = StartPoint + MeshTransform.TransformVector(FVector(0.0f,CurrentPoseArray[FeatureOffset], 0.0f));

	AnimInstanceProxy->AnimDrawDebugCircle(MeshLocation, 50.0f, 32, DebugColor, FVector::UpVector,
		false, -1.0f, SDPG_Foreground, 2.5f);
	AnimInstanceProxy->AnimDrawDebugDirectionalArrow(StartPoint, EndPoint, 60.0f, DebugColor, false,
		-1.0f, 3.0f, SDPG_Foreground);
}
#endif