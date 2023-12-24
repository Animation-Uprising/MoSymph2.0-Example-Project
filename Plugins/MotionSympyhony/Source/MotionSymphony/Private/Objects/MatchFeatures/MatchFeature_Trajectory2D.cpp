//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Objects/MatchFeatures/MatchFeature_Trajectory2D.h"
#include "MMPreProcessUtils.h"
#include "MotionAnimAsset.h"
#include "MotionAnimObject.h"
#include "MotionDataAsset.h"
#include "MotionMatchConfig.h"
#include "Utility/MotionMatchingUtils.h"
#include "Trajectory.h"
#include "TrajectoryGenerator.h"
#include "Animation/AnimInstanceProxy.h"


#if WITH_EDITOR
#include "Animation/DebugSkelMeshComponent.h"
#include "MotionSymphonySettings.h"
#endif


UMatchFeature_Trajectory2D::UMatchFeature_Trajectory2D()

: PastTrajectoryWeightMultiplier(1.0f),
	  DefaultDirectionWeighting(0.1f)
{
}

bool UMatchFeature_Trajectory2D::IsSetupValid() const
{
	bool bIsValid = true;
	
	//Check that there is a trajectory
	if(TrajectoryTiming.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Motion Match Config validity check failed. Trajectory2D Match feature has no trajectory points set."));
		bIsValid = false;
	}

	//Check that there are no zero time trajectory points
	bool bHasFutureTime = false;
	for(const float PointTime : TrajectoryTiming)
	{
		if(PointTime > 0.0f)
		{
			bHasFutureTime = true;
		}

		if(PointTime > -0.0001f && PointTime < 0.0001f)
		{
			UE_LOG(LogTemp, Warning, TEXT("Motion Match Config: Trajectory 2D Match feature has a trajectory point with time '0' (zero) which should be avoided"));
		}
	}

	//Check that there is at least 1 future trajectory point
	if(!bHasFutureTime)
	{
		UE_LOG(LogTemp, Error, TEXT("Motion Match Config: Trajectory2D Match feature must have at least one future trajectory point"));
		bIsValid = false;
	}

	return bIsValid;
}

int32 UMatchFeature_Trajectory2D::Size() const
{
	return TrajectoryTiming.Num() * 4;
}

void UMatchFeature_Trajectory2D::EvaluatePreProcess(float* ResultLocation, UAnimSequence* InSequence,
                                                    const float Time, const float PoseInterval, const bool bMirror,
                                                    UMirrorDataTable* MirrorDataTable, TObjectPtr<UMotionAnimObject> InMotionObject)
{
	if(!InSequence)
	{
		*ResultLocation = 0.0f;
		for(int32 i = 1; i < Size(); ++i)
		{
			++ResultLocation;
			*ResultLocation = 0.0f;
		}

		return;
	}

	//Extract User Data
	bool bLoop = false;
	float PlayRate = 1.0f;
	ETrajectoryPreProcessMethod PastTrajectoryMethod = ETrajectoryPreProcessMethod::Extrapolate;
	ETrajectoryPreProcessMethod FutureTrajectoryMethod = ETrajectoryPreProcessMethod::Extrapolate;
	UAnimSequence* PrecedingMotion = nullptr;
	UAnimSequence* FollowingMotion = nullptr;
	if(InMotionObject)
	{

			bLoop = InMotionObject->bLoop;
			PlayRate = InMotionObject->GetPlayRate();
			PastTrajectoryMethod = InMotionObject->PastTrajectory;
			FutureTrajectoryMethod = InMotionObject->FutureTrajectory;
			PrecedingMotion = InMotionObject->PrecedingMotion;
			FollowingMotion = InMotionObject->FollowingMotion;
		
	}
	
	//--ResultLocation;
	for(const float PointTime : TrajectoryTiming)
	{
		FTrajectoryPoint TrajectoryPoint;
		if(bLoop)
		{
			FMMPreProcessUtils::ExtractLoopingTrajectoryPoint(TrajectoryPoint, InSequence, Time,
				PointTime * PlayRate);
		}
		else if(PointTime < 0.0f) //Past Trajectory Point
		{
			FMMPreProcessUtils::ExtractPastTrajectoryPoint(TrajectoryPoint, InSequence, Time,
				PointTime * PlayRate, PastTrajectoryMethod, PrecedingMotion);
		}
		else //Future Trajectory Point
		{
			FMMPreProcessUtils::ExtractFutureTrajectoryPoint(TrajectoryPoint, InSequence, Time,
				PointTime * PlayRate, FutureTrajectoryMethod, FollowingMotion);
		}

		FVector2D FacingVector(FMath::Cos(FMath::DegreesToRadians(TrajectoryPoint.RotationZ)),
		FMath::Sin(FMath::DegreesToRadians(TrajectoryPoint.RotationZ)));
		FacingVector = FacingVector.GetSafeNormal() * 100.0f;
		
		*ResultLocation = static_cast<float>(bMirror ? -TrajectoryPoint.Position.X : TrajectoryPoint.Position.X);
		++ResultLocation;
		*ResultLocation = static_cast<float>(TrajectoryPoint.Position.Y);
		++ResultLocation;
		
		*ResultLocation = FacingVector.X;
		++ResultLocation;
		*ResultLocation = bMirror ? -FacingVector.Y : FacingVector.Y;
		++ResultLocation;
	}
}

void UMatchFeature_Trajectory2D::EvaluatePreProcess(float* ResultLocation, UAnimComposite* InComposite,
                                                    const float Time, const float PoseInterval, const bool bMirror,
                                                    UMirrorDataTable* MirrorDataTable, TObjectPtr<UMotionAnimObject> InMotionObject)
{
	if(!InComposite)
	{
		*ResultLocation = 0.0f;
		for(int32 i = 1; i < Size(); ++i)
		{
			++ResultLocation;
			*ResultLocation = 0.0f;
		}

		return;
	}

	//Extract User Data
	bool bLoop = false;
	float PlayRate = 1.0f;
	ETrajectoryPreProcessMethod PastTrajectoryMethod = ETrajectoryPreProcessMethod::Extrapolate;
	ETrajectoryPreProcessMethod FutureTrajectoryMethod = ETrajectoryPreProcessMethod::Extrapolate;
	UAnimSequence* PrecedingMotion = nullptr;
	UAnimSequence* FollowingMotion = nullptr;
	if(InMotionObject)
	{
		bLoop = InMotionObject->bLoop;
		PlayRate = InMotionObject->GetPlayRate();
		PastTrajectoryMethod = InMotionObject->PastTrajectory;
		FutureTrajectoryMethod = InMotionObject->FutureTrajectory;
		PrecedingMotion = InMotionObject->PrecedingMotion;
		FollowingMotion = InMotionObject->FollowingMotion;
	}
	
	for(const float PointTime : TrajectoryTiming)
	{
		FTrajectoryPoint TrajectoryPoint;
		if(bLoop)
		{
			FMMPreProcessUtils::ExtractLoopingTrajectoryPoint(TrajectoryPoint, InComposite,
				Time, PointTime * PlayRate);
		}
		else if(PointTime < 0.0f) //Past Trajectory Point
			{
			FMMPreProcessUtils::ExtractPastTrajectoryPoint(TrajectoryPoint, InComposite, Time,
				PointTime * PlayRate, PastTrajectoryMethod, PrecedingMotion);
			}
		else //Future Trajectory Point
			{
			FMMPreProcessUtils::ExtractFutureTrajectoryPoint(TrajectoryPoint, InComposite, Time,
				PointTime * PlayRate, FutureTrajectoryMethod, FollowingMotion);
			}

		FVector2D FacingVector(FMath::Cos(FMath::DegreesToRadians(TrajectoryPoint.RotationZ)),
		FMath::Sin(FMath::DegreesToRadians(TrajectoryPoint.RotationZ)));
		FacingVector = FacingVector.GetSafeNormal() * 100.0f;
		
		*ResultLocation = static_cast<float>(bMirror ? -TrajectoryPoint.Position.X : TrajectoryPoint.Position.X);
		++ResultLocation;
		*ResultLocation = static_cast<float>(TrajectoryPoint.Position.Y);
		++ResultLocation;
		*ResultLocation = FacingVector.X;
		++ResultLocation;
		*ResultLocation = bMirror ? -FacingVector.Y : FacingVector.Y;
		++ResultLocation;
	}
}

void UMatchFeature_Trajectory2D::EvaluatePreProcess(float* ResultLocation, UBlendSpace* InBlendSpace,
                                                    const float Time, const float PoseInterval, const bool bMirror,
                                                    UMirrorDataTable* MirrorDataTable, const FVector2D BlendSpacePosition, TObjectPtr<UMotionAnimObject> InMotionObject)
{
	if(!InBlendSpace)
	{
		*ResultLocation = 0.0f;
		for(int32 i = 1; i < Size(); ++i)
		{
			++ResultLocation;
			*ResultLocation = 0.0f;
		}

		return;
	}

	TArray<FBlendSampleData> SampleDataList;
	int32 CachedTriangulationIndex = -1;
	InBlendSpace->GetSamplesFromBlendInput(FVector(BlendSpacePosition.X, BlendSpacePosition.Y, 0.0f),
		SampleDataList, CachedTriangulationIndex, false);

	//Extract User Data
	bool bLoop = false;
	float PlayRate = 1.0f;
	ETrajectoryPreProcessMethod PastTrajectoryMethod = ETrajectoryPreProcessMethod::Extrapolate;
	ETrajectoryPreProcessMethod FutureTrajectoryMethod = ETrajectoryPreProcessMethod::Extrapolate;
	UAnimSequence* PrecedingMotion = nullptr;
	UAnimSequence* FollowingMotion = nullptr;
	if(InMotionObject)
	{
		bLoop = InMotionObject->bLoop;
		PlayRate = InMotionObject->PlayRate;
		PastTrajectoryMethod = InMotionObject->PastTrajectory;
		FutureTrajectoryMethod = InMotionObject->FutureTrajectory;
		PrecedingMotion = InMotionObject->PrecedingMotion;
		FollowingMotion = InMotionObject->FollowingMotion;
	}
	
	for(const float PointTime : TrajectoryTiming)
	{
		FTrajectoryPoint TrajectoryPoint;
		if(bLoop)
		{
			FMMPreProcessUtils::ExtractLoopingTrajectoryPoint(TrajectoryPoint, SampleDataList, Time,
				PointTime * PlayRate);
		}
		else if(PointTime < 0.0f) //Past Trajectory Point
		{
			FMMPreProcessUtils::ExtractPastTrajectoryPoint(TrajectoryPoint, SampleDataList, Time,
				PointTime * PlayRate, PastTrajectoryMethod, PrecedingMotion);
		}
		else //Future Trajectory Point
		{
			FMMPreProcessUtils::ExtractFutureTrajectoryPoint(TrajectoryPoint, SampleDataList, Time,
				PointTime * PlayRate, FutureTrajectoryMethod, FollowingMotion);
		}

		FVector2D FacingVector(FMath::Cos(FMath::DegreesToRadians(TrajectoryPoint.RotationZ)),
		FMath::Sin(FMath::DegreesToRadians(TrajectoryPoint.RotationZ)));
		FacingVector = FacingVector.GetSafeNormal() * 100.0f;
		
		*ResultLocation = static_cast<float>(bMirror ? -TrajectoryPoint.Position.X : TrajectoryPoint.Position.X);
		++ResultLocation;
		*ResultLocation = static_cast<float>(TrajectoryPoint.Position.Y);
		++ResultLocation;
		*ResultLocation = FacingVector.X;
		++ResultLocation;
		*ResultLocation = bMirror ? -FacingVector.Y : FacingVector.Y;
		++ResultLocation;
	}
}

void UMatchFeature_Trajectory2D::SourceInputData(TArray<float>& OutFeatureArray, const int32 FeatureOffset, AActor* InActor)
{
	if(!InActor)
	{
		UMatchFeatureBase::SourceInputData(OutFeatureArray, FeatureOffset, nullptr);
		return;
	}

	if(UTrajectoryGenerator_Base* TrajectoryGenerator = InActor->GetComponentByClass<UTrajectoryGenerator_Base>())
	{
		const FTrajectory& Trajectory = TrajectoryGenerator->GetCurrentTrajectory();

		const int32 Iterations = FMath::Min(TrajectoryTiming.Num(), Trajectory.TrajectoryPoints.Num());
		
		for(int32 i = 0; i < Iterations; ++i)
		{
			const FTrajectoryPoint& TrajectoryPoint = Trajectory.TrajectoryPoints[i];

			FVector RotationVector = FQuat(FVector::UpVector,
				FMath::DegreesToRadians(TrajectoryPoint.RotationZ)) * FVector::ForwardVector;
			RotationVector = RotationVector.GetSafeNormal() * 100.0f;

			const int32 PointOffset = FeatureOffset + (i * 4.0f);

			if(PointOffset + 3 >= OutFeatureArray.Num())
			{
				UE_LOG(LogTemp, Error, TEXT("UMatchFeature_Trajectory2D: SourceInputData(...) - Feature does not fit in FeatureArray"));
				return;
			}
			
			OutFeatureArray[PointOffset] = TrajectoryPoint.Position.X;
			OutFeatureArray[PointOffset + 1] = TrajectoryPoint.Position.Y;
			OutFeatureArray[PointOffset + 2] = RotationVector.X;
			OutFeatureArray[PointOffset + 3] = RotationVector.Y;
		}
	}
}

void UMatchFeature_Trajectory2D::ApplyInputBlending(TArray<float>& DesiredInputArray,
                                                    const TArray<float>& CurrentPoseArray, const int32 FeatureOffset, const float Weight)
{
	if(DesiredInputArray.Num() < FeatureOffset + TrajectoryTiming.Num() * 4
		|| CurrentPoseArray.Num() != DesiredInputArray.Num())
	{
		return;
	}

	const float TotalTime = FMath::Max(0.00001f, TrajectoryTiming.Last());
	
	for(int32 i = 0; i < TrajectoryTiming.Num(); ++i)
	{
		const int32 StartIndex = FeatureOffset + i * 4;
		const float Time = TrajectoryTiming[i];

		if(Time > 0.0f)
		{
			const float Progress = ((TotalTime - Time) / TotalTime) * Weight;
			DesiredInputArray[StartIndex] = FMath::Lerp(CurrentPoseArray[StartIndex], DesiredInputArray[StartIndex], Progress);
			DesiredInputArray[StartIndex+1] = FMath::Lerp(CurrentPoseArray[StartIndex+1], DesiredInputArray[StartIndex+1], Progress);
			DesiredInputArray[StartIndex+2] = FMath::Lerp(CurrentPoseArray[StartIndex+2], DesiredInputArray[StartIndex+2], Progress);
			DesiredInputArray[StartIndex+3] = FMath::Lerp(CurrentPoseArray[StartIndex+3], DesiredInputArray[StartIndex+3], Progress);
		}
	}
}

bool UMatchFeature_Trajectory2D::NextPoseToleranceTest(const TArray<float>& DesiredInputArray, const TArray<float>& PoseMatrix,
	const int32 MatrixStartIndex, const int32 FeatureOffset, const float PositionTolerance, const float RotationTolerance)
{
	const int32 Iterations = TrajectoryTiming.Num();

	if(PoseMatrix.Num() <= MatrixStartIndex + Iterations * 4 + 1
		|| DesiredInputArray.Num() <= FeatureOffset + Iterations * 4)
	{
		return false;
	}
	
	for(int32 i = 0; i < Iterations; ++i)
	{
		const int32 PointIndex = FeatureOffset + i * 4 - 1;
		const int32 MatrixIndex = MatrixStartIndex + i * 4;
		
		const float PredictionTime = TrajectoryTiming[i];
		
		const float RelativeTolerance_Pos = PredictionTime * PositionTolerance;
		const float RelativeTolerance_Rot = PredictionTime * RotationTolerance;
		
		float SqrDistance = FMath::Abs(DesiredInputArray[PointIndex] - PoseMatrix[MatrixIndex]);
		SqrDistance += FMath::Abs(DesiredInputArray[PointIndex+1] - PoseMatrix[MatrixIndex+1]);

		if(SqrDistance > RelativeTolerance_Pos * RelativeTolerance_Pos)
		{
			return false;
		}

		SqrDistance = FMath::Abs(DesiredInputArray[PointIndex + 2] - PoseMatrix[MatrixIndex + 2]);
		SqrDistance += FMath::Abs(DesiredInputArray[PointIndex + 3] - PoseMatrix[MatrixIndex + 3]);

		if(SqrDistance > RelativeTolerance_Rot * RelativeTolerance_Rot)
		{
			return false;
		}
	}

	return true;
}

float UMatchFeature_Trajectory2D::GetDefaultWeight(int32 AtomId) const
{
	const int32 SetId = FMath::FloorToInt32(static_cast<float>(AtomId) / 4.0f);
	const int32 TrajPointAtomId = AtomId - (SetId * 4);

	float WeightMultiplier = PastTrajectoryWeightMultiplier;
	const int32 MaxIterations = FMath::Min(SetId + 1, TrajectoryTiming.Num());
	for(int32 i = 0; i <  MaxIterations; ++i)
	{
		if(TrajectoryTiming[i] > 0)
		{
			WeightMultiplier = 1.0f;
		}
	}

	if(TrajPointAtomId < 2)
	{
		return DefaultWeight * WeightMultiplier;
	}
	else
	{
		return DefaultDirectionWeighting * WeightMultiplier;
	}
}

void UMatchFeature_Trajectory2D::CalculateDistanceSqrToMeanArrayForStandardDeviations(TArray<float>& OutDistToMeanSqrArray,
	const TArray<float>& InMeanArray, const TArray<float>& InPoseArray, const int32 FeatureOffset, const int32 PoseStartIndex) const
{
	for(int32 i = 0; i < TrajectoryTiming.Num(); ++i)
	{
		const int32 MeansStartIndex = FeatureOffset + i*4;
		const int32 PointStartIndex = PoseStartIndex + MeansStartIndex;
		
		FVector2d PointLocation
		{
			InPoseArray[PointStartIndex],
			InPoseArray[PointStartIndex + 1]
		};

		FVector2d MeanPointLocation
		{
			InMeanArray[MeansStartIndex],
			InMeanArray[MeansStartIndex + 1]
		};
	
		FVector2d PointFacing
		{
			InPoseArray[PointStartIndex + 2],
			InPoseArray[PointStartIndex + 3]
		};

		FVector2d MeanPointFacing
		{
			InMeanArray[MeansStartIndex + 2],
			InMeanArray[MeansStartIndex + 3]
		};
		
		const float DistanceToMean_Location = FVector2d::DistSquared(PointLocation, MeanPointLocation);
		const float DistanceToMean_Facing = FVector2d::DistSquared(PointFacing, MeanPointFacing);

		OutDistToMeanSqrArray[MeansStartIndex] += DistanceToMean_Location;
		OutDistToMeanSqrArray[MeansStartIndex + 1] += DistanceToMean_Location;
		OutDistToMeanSqrArray[MeansStartIndex + 2] += DistanceToMean_Facing;
		OutDistToMeanSqrArray[MeansStartIndex + 3] += DistanceToMean_Facing;
	}
}

bool UMatchFeature_Trajectory2D::CanBeResponseFeature() const
{
	return true;
}

#if WITH_EDITOR
void UMatchFeature_Trajectory2D::DrawPoseDebugEditor(UMotionDataAsset* MotionData, UDebugSkelMeshComponent* DebugSkeletalMesh,
                                                     const int32 PreviewIndex, const int32 FeatureOffset, const UWorld* World,
                                                     FPrimitiveDrawInterface* DrawInterface)
{
	if(!MotionData
		|| !DebugSkeletalMesh
		|| !World)
	{
		return;
	}
	
	TArray<float>& PoseArray = MotionData->LookupPoseMatrix.PoseArray;
	const int32 StartIndex = PreviewIndex * MotionData->LookupPoseMatrix.AtomCount + FeatureOffset;
	
	if(PoseArray.Num() < StartIndex + Size())
	{
		return;
	}

	const FTransform PreviewTransform = DebugSkeletalMesh->GetComponentTransform();
	
	FVector LastPointPos = PreviewTransform.TransformPosition(FVector(PoseArray[StartIndex], PoseArray[StartIndex+1], 0.0f));
	
	const FQuat FacingOffset(FVector::UpVector, FMath::DegreesToRadians(
		FMotionMatchingUtils::GetFacingAngleOffset(MotionData->MotionMatchConfig->ForwardAxis)));

	const UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();
	const FColor DebugColor = Settings->DebugColor_Trajectory;
	const FLinearColor LinearDebugColor = Settings->DebugColor_Trajectory;
	
	bool bHasPastFutureLineBeenCrossed = false;
	for(int32 i = 0; i < TrajectoryTiming.Num(); ++i)
	{
		//This is just to add a 'virtual' trajectory point at zero time between the past and future trajectories
		if(!bHasPastFutureLineBeenCrossed && TrajectoryTiming[i] > 0.0f)
		{
			bHasPastFutureLineBeenCrossed = true;
			const FVector PreviewLocation = PreviewTransform.GetLocation();
			
			DrawInterface->DrawLine(LastPointPos, PreviewLocation, LinearDebugColor,
				SDPG_Foreground, 3.0f);

			LastPointPos = PreviewLocation;
		}
		
		const int32 PointIndex = StartIndex + i*4;
		
		FVector RawPointPos(PoseArray[PointIndex], PoseArray[PointIndex + 1], 0.0f);
		FVector PointPos = PreviewTransform.TransformPosition(RawPointPos);

		DrawDebugSphere(World, PointPos, 5.0f, 8, DebugColor, true, -1, -1);

		DrawInterface->DrawLine(LastPointPos, PointPos, LinearDebugColor,
			SDPG_Foreground, 3.0f);

		FVector RawArrowVector(PoseArray[PointIndex + 2], PoseArray[PointIndex + 3], 0.0f);
		FVector ArrowVector = PreviewTransform.TransformVector(FacingOffset * RawArrowVector) / 3.0f;

		DrawDebugDirectionalArrow(World, PointPos, PointPos + ArrowVector, 20.0f, DebugColor,
			true, -1, -1, 1.5f);

		LastPointPos = PointPos;
	}
}

void UMatchFeature_Trajectory2D::DrawDebugDesiredRuntime(FAnimInstanceProxy* AnimInstanceProxy,
	FMotionMatchingInputData& InputData, const int32 FeatureOffset, UMotionMatchConfig* MMConfig)
{
	if(!AnimInstanceProxy)
	{
		return;
	}
	
	const FTransform& MeshTransform = AnimInstanceProxy->GetSkelMeshComponent()->GetComponentTransform();
	const FVector ActorLocation = MeshTransform.GetLocation();

	TArray<float>& InputArray = InputData.DesiredInputArray;
	
	const FQuat FacingOffset(FVector::UpVector, FMath::DegreesToRadians(
		FMotionMatchingUtils::GetFacingAngleOffset(MMConfig->ForwardAxis)));

	const UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();
	
	FColor DebugColor;
	FVector LastPoint;
	for(int32 i = 0; i < TrajectoryTiming.Num(); ++i)
	{
		const int32 StartIndex = FeatureOffset + i * 4 - 1;
		
		if(StartIndex + 3 >= InputArray.Num())
		{
			break;
		}
	
		if(TrajectoryTiming[i] < 0.0f)
		{
			DebugColor = Settings->DebugColor_DesiredTrajectoryPast;
		}
		else
		{
			DebugColor = Settings->DebugColor_DesiredTrajectory;
		}
		
		FVector PointPosition = MeshTransform.TransformPosition(FVector(InputArray[StartIndex], InputArray[StartIndex+1], 0.0f));

		AnimInstanceProxy->AnimDrawDebugSphere(PointPosition, 5.0f, 8, DebugColor, false,
			-1.0f, 0.0f, SDPG_Foreground);
		
		FVector DrawTo = PointPosition + MeshTransform.TransformVector(FacingOffset *
			FVector(InputArray[StartIndex+2], InputArray[StartIndex+3], 0.0f) / 3.0f);
		
		AnimInstanceProxy->AnimDrawDebugDirectionalArrow(PointPosition, DrawTo, 40.0f, DebugColor,
			false, -1.0f, 2.0f, SDPG_Foreground);
	
		if(i > 0) 
		{
			if(TrajectoryTiming[i - 1] < 0.0f && TrajectoryTiming[i] > 0.0f)
			{
				AnimInstanceProxy->AnimDrawDebugLine(LastPoint, ActorLocation,
					Settings->DebugColor_DesiredTrajectoryPast, false, -1.0f, 2.0f, SDPG_Foreground);
				AnimInstanceProxy->AnimDrawDebugLine(ActorLocation, PointPosition,
					Settings->DebugColor_DesiredTrajectory, false, -1.0f, 2.0f, SDPG_Foreground);
				AnimInstanceProxy->AnimDrawDebugSphere(ActorLocation, 5.0f, 8,
					Settings->DebugColor_DesiredTrajectory, false, -1.0f, SDPG_Foreground);
			}
			else
			{
				AnimInstanceProxy->AnimDrawDebugLine(LastPoint, PointPosition, DebugColor, false,
					-1.0f, 2.0f, SDPG_Foreground);
			}
		}
	
		LastPoint = PointPosition;
	}
}

void UMatchFeature_Trajectory2D::DrawDebugCurrentRuntime(FAnimInstanceProxy* AnimInstanceProxy,
                                                         TObjectPtr<const UMotionDataAsset> MotionData, const TArray<float>& CurrentPoseArray, const int32 FeatureOffset)
{
	if(!AnimInstanceProxy
		|| !MotionData
		||  TrajectoryTiming.Num() == 0)
	{
		return;
	}

	const UMotionMatchConfig* MMConfig = MotionData->MotionMatchConfig;
	
	const FTransform& MeshTransform = AnimInstanceProxy->GetSkelMeshComponent()->GetComponentTransform();
	const FVector ActorLocation = MeshTransform.GetLocation();
	FVector LastPoint;
	
	const FQuat FacingOffset(FVector::UpVector, FMath::DegreesToRadians(
		FMotionMatchingUtils::GetFacingAngleOffset(MMConfig->ForwardAxis)));

	const UMotionSymphonySettings* Settings = GetMutableDefault<UMotionSymphonySettings>();
	FColor DebugColor;

	for(int32 i = 0; i < TrajectoryTiming.Num(); ++i)
	{
		const int32 StartIndex = FeatureOffset + i * 4;
		
		if(StartIndex + 3 >= CurrentPoseArray.Num()) //safety
		{
			break;
		}

		if(TrajectoryTiming[i] < 0.0f)
		{
			DebugColor = Settings->DebugColor_TrajectoryPast;
		}
		else
		{
			DebugColor = Settings->DebugColor_Trajectory;
		}

		FVector PointPosition = MeshTransform.TransformPosition(FVector(CurrentPoseArray[StartIndex],
			CurrentPoseArray[StartIndex+1], 0.0f));
		AnimInstanceProxy->AnimDrawDebugSphere(PointPosition, 5.0f, 8, DebugColor, false,
			-1.0f, 0.0f, SDPG_Foreground);
		
		FVector DrawTo = PointPosition + MeshTransform.TransformVector(FacingOffset *
			FVector(CurrentPoseArray[StartIndex+2], CurrentPoseArray[StartIndex+3], 0.0f) / 3.0f);
		
		AnimInstanceProxy->AnimDrawDebugDirectionalArrow(PointPosition, DrawTo, 40.0f, DebugColor,
			false, -1.0f, 2.0f, SDPG_Foreground);

		if(i > 0) 
		{
			if(TrajectoryTiming[i - 1] < 0.0f && TrajectoryTiming[i] > 0.0f)
			{
				AnimInstanceProxy->AnimDrawDebugLine(LastPoint, ActorLocation,
					Settings->DebugColor_TrajectoryPast, false, -1.0f, 2.0f, SDPG_Foreground);
				AnimInstanceProxy->AnimDrawDebugLine(ActorLocation, PointPosition,
					Settings->DebugColor_Trajectory, false, -1.0f, 2.0f, SDPG_Foreground);
				AnimInstanceProxy->AnimDrawDebugSphere(ActorLocation, 5.0f, 8,
					Settings->DebugColor_Trajectory, false, -1.0f, SDPG_Foreground);
			}
			else
			{
				AnimInstanceProxy->AnimDrawDebugLine(LastPoint, PointPosition, DebugColor, false,
					-1.0f, 2.0f, SDPG_Foreground);
			}
		}

		LastPoint = PointPosition;
	}
}

#endif