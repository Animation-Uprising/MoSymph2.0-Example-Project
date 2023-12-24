//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Objects/MatchFeatures/MatchFeature_BodyMomentum3D.h"
#include "Animation/AnimInstanceProxy.h"
#include "MMPreProcessUtils.h"
#include "MotionAnimAsset.h"
#include "Objects/MotionAnimObject.h"
#include "MotionDataAsset.h"
#include "GameFramework/CharacterMovementComponent.h"

#if WITH_EDITOR
#include "Animation/DebugSkelMeshComponent.h"
#include "MotionSymphonySettings.h"
#endif

int32 UMatchFeature_BodyMomentum3D::Size() const
{
	return 3;
}

void UMatchFeature_BodyMomentum3D::EvaluatePreProcess(float* ResultLocation, UAnimSequence* InSequence,
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
	}

	FVector RootVelocity;
	float RootRotVelocity;
	FMMPreProcessUtils::ExtractRootVelocity(RootVelocity, RootRotVelocity, InSequence, Time, PoseInterval);
	
	if(InMotionObject)
	{
		RootVelocity *= InMotionObject->GetPlayRate();
	}

	*ResultLocation = bMirror ? -RootVelocity.X : RootVelocity.X;
	++ResultLocation;
	*ResultLocation = RootVelocity.Y;
	++ResultLocation;
	*ResultLocation = RootVelocity.Z;
}

void UMatchFeature_BodyMomentum3D::EvaluatePreProcess(float* ResultLocation, UAnimComposite* InComposite,
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
		return;
	}
	
	FVector RootVelocity;
	float RootRotVelocity;
	FMMPreProcessUtils::ExtractRootVelocity(RootVelocity, RootRotVelocity, InComposite, Time, PoseInterval);

	if(InMotionObject)
	{
		RootVelocity *= InMotionObject->GetPlayRate();
		
	}
	
	*ResultLocation = bMirror ? -RootVelocity.X : RootVelocity.X;
	++ResultLocation;
	*ResultLocation = RootVelocity.Y;
	++ResultLocation;
	*ResultLocation = RootVelocity.Z;
}

void UMatchFeature_BodyMomentum3D::EvaluatePreProcess(float* ResultLocation, UBlendSpace* InBlendSpace,
                                                      const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable,
                                                      const FVector2D BlendSpacePosition, TObjectPtr<UMotionAnimObject> InMotionObject)
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
	
	TArray<FBlendSampleData> SampleDataList;
	int32 CachedTriangulationIndex = -1;
	InBlendSpace->GetSamplesFromBlendInput(FVector(BlendSpacePosition.X, BlendSpacePosition.Y, 0.0f),
		SampleDataList, CachedTriangulationIndex, false);
	
	FVector RootVelocity;
	float RootRotVelocity;
	FMMPreProcessUtils::ExtractRootVelocity(RootVelocity, RootRotVelocity, SampleDataList, Time, PoseInterval);
	
	if(InMotionObject)
	{
		RootVelocity *= InMotionObject->GetPlayRate();
	}
	
	*ResultLocation = bMirror ? -RootVelocity.X : RootVelocity.X;
	++ResultLocation;
	*ResultLocation = RootVelocity.Y;
	++ResultLocation;
	*ResultLocation = RootVelocity.Z;
}

void UMatchFeature_BodyMomentum3D::ExtractRuntime(FCSPose<FCompactPose>& CSPose, float* ResultLocation,
	float* FeatureCacheLocation, FAnimInstanceProxy* AnimInstanceProxy, float DeltaTime)
{
	if(!AnimInstanceProxy)
	{
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;
		++ResultLocation;
		*ResultLocation = 0.0f;

		*FeatureCacheLocation = 0.0f;
		++FeatureCacheLocation;
		*FeatureCacheLocation = 0.0f;
		++FeatureCacheLocation;
		*FeatureCacheLocation = 0.0f;
	}
	
	const FVector ActorLocation = AnimInstanceProxy->GetActorTransform().GetLocation();
	const FVector LastActorLocation(*FeatureCacheLocation, *FeatureCacheLocation+1, *FeatureCacheLocation+2);
	const FVector Velocity = (FVector(ActorLocation.X, ActorLocation.Y, ActorLocation.Z) - LastActorLocation) / FMath::Max(0.00001f, DeltaTime);
	
	//Save the Bone Location for velocity calculation next frame
	*FeatureCacheLocation = ActorLocation.X;
	++FeatureCacheLocation;
	*FeatureCacheLocation = ActorLocation.Y;
	++FeatureCacheLocation;
	*FeatureCacheLocation = ActorLocation.Z;
	
	//Save the velocity to the result location
	*ResultLocation = Velocity.X;
	++ResultLocation;
	*ResultLocation = Velocity.Y;
	++ResultLocation;
	*ResultLocation = Velocity.Z;
}

void UMatchFeature_BodyMomentum3D::SourceInputData(TArray<float>& OutFeatureArray, const int32 FeatureOffset,
	AActor* InActor)
{
	if(!InActor)
	{
		UMatchFeatureBase::SourceInputData(OutFeatureArray, FeatureOffset, nullptr);
		return;
	}

	if(UCharacterMovementComponent* MovementComponent = InActor->GetComponentByClass<UCharacterMovementComponent>())
	{
		const FVector Velocity = InActor->GetActorTransform().TransformVector(MovementComponent->Velocity);

		OutFeatureArray[FeatureOffset] = Velocity.X;
		OutFeatureArray[FeatureOffset + 1] = Velocity.Y;
		OutFeatureArray[FeatureOffset + 2] = Velocity.Z;
	}
}

bool UMatchFeature_BodyMomentum3D::NextPoseToleranceTest(const TArray<float>& DesiredInputArray,
	const TArray<float>& PoseMatrix, const int32 MatrixStartIndex, const int32 FeatureOffset,
	const float PositionTolerance, const float RotationTolerance)
{
	if(DesiredInputArray.Num() <= FeatureOffset + 1
		|| PoseMatrix.Num() <= MatrixStartIndex + 2)
	{
		return false;
	}
	
	const float SqrDistance = FMath::Abs(DesiredInputArray[FeatureOffset - 1] - PoseMatrix[MatrixStartIndex])
		+ FMath::Abs(DesiredInputArray[FeatureOffset] - PoseMatrix[MatrixStartIndex + 1])
		+ FMath::Abs(DesiredInputArray[FeatureOffset + 1] - PoseMatrix[MatrixStartIndex + 2]);

	if(SqrDistance > PositionTolerance * PositionTolerance)
	{
		return false;
	}

	return true;
}

void UMatchFeature_BodyMomentum3D::CalculateDistanceSqrToMeanArrayForStandardDeviations(
	TArray<float>& OutDistToMeanSqrArray, const TArray<float>& InMeanArray, const TArray<float>& InPoseArray,
	const int32 FeatureOffset, const int32 PoseStartIndex) const
{
	const FVector Momentum
	{
		InPoseArray[PoseStartIndex + FeatureOffset],
		InPoseArray[PoseStartIndex + FeatureOffset + 1],
		InPoseArray[PoseStartIndex + FeatureOffset + 2]
	};

	const FVector MeanMomentum
	{
		InMeanArray[FeatureOffset],
		InMeanArray[FeatureOffset + 1],
		InMeanArray[FeatureOffset + 2]
	};

	const float DistanceToMean = FVector::DistSquared(Momentum, MeanMomentum);

	OutDistToMeanSqrArray[FeatureOffset] += DistanceToMean;
	OutDistToMeanSqrArray[FeatureOffset+1] += DistanceToMean;
	OutDistToMeanSqrArray[FeatureOffset+1] += DistanceToMean;
}

bool UMatchFeature_BodyMomentum3D::CanBeQualityFeature() const
{
	return true;
}

bool UMatchFeature_BodyMomentum3D::CanBeResponseFeature() const
{
	return true;
}

#if WITH_EDITOR
void UMatchFeature_BodyMomentum3D::DrawPoseDebugEditor(UMotionDataAsset* MotionData,
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
	const FColor DebugColor = Settings->DebugColor_BodyVelocity;

	const FTransform PreviewTransform = DebugSkeletalMesh->GetComponentTransform();
	const FVector StartPoint = PreviewTransform.GetLocation();
	const FVector EndPoint = StartPoint + PreviewTransform.TransformVector(FVector(PoseArray[StartIndex],
		PoseArray[StartIndex+1], 0.0f));

	DrawDebugCircle(World, StartPoint, 25.0f, 32, DebugColor, true, -1, -1,
		1.5, FVector::LeftVector, FVector::ForwardVector, false);
	DrawDebugDirectionalArrow(World, StartPoint , EndPoint, 20.0f, DebugColor, true, -1, -1, 1.5f);
}

void UMatchFeature_BodyMomentum3D::DrawDebugCurrentRuntime(FAnimInstanceProxy* AnimInstanceProxy,
                                                           TObjectPtr<const UMotionDataAsset> MotionData, const TArray<float>& CurrentPoseArray, const int32 FeatureOffset)
{
	if(!AnimInstanceProxy || !MotionData)
	{
		return;
	}

	const UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();
	const FColor DebugColor = Settings->DebugColor_BodyVelocity;

	//const FTransform& ActorTransform = AnimInstanceProxy->GetActorTransform();
	const FTransform& MeshTransform = AnimInstanceProxy->GetSkelMeshComponent()->GetComponentTransform();
	const FVector MeshLocation = MeshTransform.GetLocation();

	AnimInstanceProxy->AnimDrawDebugCircle(MeshLocation, 25.0f, 32, DebugColor, FVector::UpVector,
		false, -1.0f, SDPG_Foreground, 2.5f);
	//AnimInstanceProxy->AnimDrawDebugSphere(MeshLocation, 5.0f, 32, DebugColor, false, -1.0f, 0.0f);
	AnimInstanceProxy->AnimDrawDebugDirectionalArrow(MeshLocation, MeshLocation + MeshTransform.TransformVector(
		FVector(CurrentPoseArray[FeatureOffset], CurrentPoseArray[FeatureOffset+1], MeshLocation.Z)),
		80.0f, DebugColor, false, -1.0f, 3.0f, SDPG_Foreground);
}
#endif