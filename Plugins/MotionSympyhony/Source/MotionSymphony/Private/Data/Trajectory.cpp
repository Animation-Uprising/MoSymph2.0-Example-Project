//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Data/Trajectory.h"
#include "MotionSymphony.h"

FTrajectory::FTrajectory()
{
}

FTrajectory::~FTrajectory()
{
	TrajectoryPoints.Empty();
}

void FTrajectory::Initialize(int TrajectoryCount)
{
	TrajectoryPoints.Empty(TrajectoryCount);
	
	for (int i = 0; i < TrajectoryCount; ++i)
	{
		TrajectoryPoints.Emplace(FTrajectoryPoint());
	}
}

void FTrajectory::Clear()
{
	TrajectoryPoints.Empty();
}

void FTrajectory::MakeRelativeTo(FTransform Transform)
{
	const float ModelYaw = Transform.Rotator().Yaw + 90.0f; //-90.0f is to make up for the 90 degree offset of characters in UE4

	for (int i = 0; i < TrajectoryPoints.Num(); ++i)
	{
		FTrajectoryPoint& point = TrajectoryPoints[i];

		const FVector NewPointPosition = Transform.InverseTransformVector(point.Position);
		float NewPointRotation = point.RotationZ - ModelYaw;

		//Wrap rotation within range -180 to 180
		float RemainingRotation = (NewPointRotation + 180.0f) / 360.0f;
		RemainingRotation -= FMath::FloorToFloat(RemainingRotation);
		NewPointRotation = RemainingRotation * 360.0f - 180.0f;

		TrajectoryPoints[i] = FTrajectoryPoint(NewPointPosition, NewPointRotation);
	}
}

void FTrajectory::SetTrajectoryPoint(const int32 Index, const FVector InPosition, const float InRotationZ)
{
	if(Index < 0 || Index > TrajectoryPoints.Num() -1)
	{
		return;
	}

	TrajectoryPoints[Index] = FTrajectoryPoint(InPosition, InRotationZ);
}

void FTrajectory::AddTrajectoryPoint(const FVector InPosition, const float InRotationZ)
{
	TrajectoryPoints.Emplace(FTrajectoryPoint(InPosition, InRotationZ));
}

int32 FTrajectory::TrajectoryPointCount() const
{
	return TrajectoryPoints.Num();
}

FMotionMatchingInputData::FMotionMatchingInputData()
{
}

FMotionMatchingInputData::~FMotionMatchingInputData()
{
}

