//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Data/TrajectoryPoint.h"
#include "MotionSymphony.h"

void FTrajectoryPoint::Lerp(FTrajectoryPoint& o_result, 
	FTrajectoryPoint& a_from, FTrajectoryPoint& a_to, float a_progress)
{
	o_result.Position = FMath::Lerp(a_from.Position, a_to.Position, a_progress);
	o_result.RotationZ = FMath::Lerp(FRotator(0.0f, a_from.RotationZ, 0.0f), 
		FRotator(0.0f, a_to.RotationZ, 0.0f), a_progress).Yaw;
}

FTrajectoryPoint::FTrajectoryPoint()
	: Position(FVector(0.0f)), RotationZ(0.0f)
{

}

FTrajectoryPoint::FTrajectoryPoint(FVector a_position, float a_facingAngle)
	: Position(a_position), RotationZ(a_facingAngle)
{

}

FTrajectoryPoint& FTrajectoryPoint::operator+=(const FTrajectoryPoint& rhs)
{
	Position += rhs.Position;
	RotationZ += rhs.RotationZ;

	return *this;
}

FTrajectoryPoint& FTrajectoryPoint::operator/=(const float rhs)
{
	Position /= rhs;
	RotationZ /= rhs;

	return *this;
}

FTrajectoryPoint& FTrajectoryPoint::operator*=(const float rhs)
{
	Position *= rhs;
	RotationZ *= rhs;

	return *this;
}