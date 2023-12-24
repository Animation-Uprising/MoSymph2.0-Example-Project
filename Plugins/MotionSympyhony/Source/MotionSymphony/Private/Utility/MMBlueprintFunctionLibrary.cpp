//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MMBlueprintFunctionLibrary.h"

#include "MotionMatchConfig.h"
#include "MotionSymphonySettings.h"
#include "Data/Trajectory.h"
#include "Camera/CameraComponent.h"

FVector UMMBlueprintFunctionLibrary::GetInputVectorRelativeToCamera(FVector InputVector, UCameraComponent* CameraComponent)
{
	if (!CameraComponent)
	{
		return InputVector;
	}

	FRotator CameraProjectedRotation = CameraComponent->GetComponentToWorld().GetRotation().Rotator();
	CameraProjectedRotation.Roll = 0.0f;
	CameraProjectedRotation.Pitch = 0.0f;

	return CameraProjectedRotation.RotateVector(InputVector);
}


FVector UMMBlueprintFunctionLibrary::GetVectorRelativeToCamera(const float InputX, const float InputY, UCameraComponent* CameraComponent)
{
	if (!CameraComponent)
	{
		return FVector(InputX, InputY, 0.0f);
	}

	FRotator CameraProjectedRotation = CameraComponent->GetComponentToWorld().GetRotation().Rotator();
	CameraProjectedRotation.Roll = 0.0f;
	CameraProjectedRotation.Pitch = 0.0f;

	return CameraProjectedRotation.RotateVector(FVector(InputX, InputY, 0.0f));
}

void UMMBlueprintFunctionLibrary::InitializeTrajectory(FTrajectory& OutTrajectory, const int32 TrajectoryCount)
{
	OutTrajectory.TrajectoryPoints.Empty(TrajectoryCount + 1);

	for (int32 i = 0; i < TrajectoryCount; ++i)
	{
		OutTrajectory.TrajectoryPoints.Emplace(FTrajectoryPoint(FVector::ZeroVector, 0.0f));
	}
}

void UMMBlueprintFunctionLibrary::SetTrajectoryPoint(FTrajectory& OutTrajectory, const int32 Index, const FVector Position, const float RotationZ)
{
	if(Index < 0 || Index > OutTrajectory.TrajectoryPointCount() - 1)
		return;

	OutTrajectory.TrajectoryPoints[Index] = FTrajectoryPoint(Position, RotationZ);
}

void UMMBlueprintFunctionLibrary::TransformFromUpForwardAxis(FTransform& OutTransform, const EAllAxis UpAxis,
                                                         const EAllAxis ForwardAxis)
{
	//If the up and forward axis are parallel it is not possible to make a 
	if(UpAxis == ForwardAxis)
	{
		UE_LOG(LogTemp, Warning, TEXT("Trying to create a transform from Up and forward vecotors but the vectors are parallel."))
		OutTransform = FTransform::Identity;
		return;
	}
	
	FVector UpAxisVector;
	switch(UpAxis)
	{
		case EAllAxis::X: { UpAxisVector = FVector::ForwardVector; } break;
		case EAllAxis::Y: { UpAxisVector = FVector::RightVector; } break;
		case EAllAxis::Z: { UpAxisVector = FVector::UpVector; } break;
		case EAllAxis::NegX: { UpAxisVector = FVector::BackwardVector; } break;
		case EAllAxis::NegY: { UpAxisVector = FVector::LeftVector; } break;
		case EAllAxis::NegZ: { UpAxisVector = FVector::DownVector; } break;
		default : { UpAxisVector = FVector::UpVector; } break;
	}

	FVector ForwardAxisVector;
	switch(ForwardAxis)
	{
		case EAllAxis::X: { ForwardAxisVector = FVector::ForwardVector; } break;
		case EAllAxis::Y: { ForwardAxisVector = FVector::RightVector; } break;
		case EAllAxis::Z: { ForwardAxisVector = FVector::UpVector; } break;
		case EAllAxis::NegX: { ForwardAxisVector = FVector::BackwardVector; } break;
		case EAllAxis::NegY: { ForwardAxisVector = FVector::LeftVector; } break;
		case EAllAxis::NegZ: { ForwardAxisVector = FVector::DownVector; } break;
		default : { ForwardAxisVector = FVector::RightVector; } break;
	}

	OutTransform = FTransform(FVector::CrossProduct(ForwardAxisVector, UpAxisVector), ForwardAxisVector,
		UpAxisVector, FVector::ZeroVector);
}

void UMMBlueprintFunctionLibrary::CreateInputDataFromTrajectory(FTrajectory& Trajectory,
	FMotionMatchingInputData& InputData)
{
	InputData.DesiredInputArray.Empty(Trajectory.TrajectoryPoints.Num() * 4);
	for(FTrajectoryPoint& TrajectoryPoint : Trajectory.TrajectoryPoints)
	{
		InputData.DesiredInputArray.Emplace(TrajectoryPoint.Position.X);
		InputData.DesiredInputArray.Emplace(TrajectoryPoint.Position.Y);
		
		FVector RotationVector = FQuat(FVector::UpVector,
			FMath::DegreesToRadians(TrajectoryPoint.RotationZ)) * FVector::ForwardVector;
		
		InputData.DesiredInputArray.Emplace(RotationVector.X);
		InputData.DesiredInputArray.Emplace(RotationVector.Y);
	}
}

void UMMBlueprintFunctionLibrary::ConstructMotionInputFeatureArray(FMotionMatchingInputData& InputData, AActor* Actor,
	UMotionMatchConfig* MotionConfig)
{
	if(!MotionConfig || !Actor)
	{
		return;
	}

	if(MotionConfig->ResponseDimensionCount == 0)
	{
		MotionConfig->Initialize();

		if(MotionConfig->ResponseDimensionCount == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("ERROR: 'ConstructMotionInputFeatureArray' node - Unable to construct input array. The motion config has an invalid setup and can't be initialized."))
		}
	}
	
	if(InputData.DesiredInputArray.Num() != MotionConfig->ResponseDimensionCount)
	{
		InputData.DesiredInputArray.SetNumZeroed(MotionConfig->ResponseDimensionCount);
	}

	int32 FeatureOffset = 0;
	for(TObjectPtr<UMatchFeatureBase> MatchFeature : MotionConfig->InputResponseFeatures)
	{
		if(MatchFeature && MatchFeature->IsSetupValid())
		{
			MatchFeature->SourceInputData(InputData.DesiredInputArray, FeatureOffset, Actor);
			FeatureOffset += MatchFeature->Size();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ERROR: 'ConstructMotionInputFeatureArray' node -  Match feature has an invalid setup and cannot be processed."))
		}
	}
}
