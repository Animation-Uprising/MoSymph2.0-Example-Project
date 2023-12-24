//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "JointData.h"

FJointData::FJointData()
	: Position(FVector(0.0f)), 
	Velocity(FVector(0.0f))
{
}

FJointData::FJointData(FVector Position, FVector Velocity)
	: Position(Position), 
	Velocity(Velocity)
{
}

FJointData::~FJointData()
{
}

void FJointData::Lerp(FJointData& OutJoint, FJointData& From, FJointData& To, float Progress)
{
	OutJoint.Position = FMath::Lerp(From.Position, To.Position, Progress);
	OutJoint.Velocity = FMath::Lerp(From.Velocity, To.Velocity, Progress);
}

FJointData& FJointData::operator+=(const FJointData& rhs)
{
	Position += rhs.Position;
	Velocity += rhs.Velocity;

	return *this;
}

FJointData& FJointData::operator/=(const float rhs)
{
	Position /= rhs;
	Velocity /= rhs;

	return *this;
}

FJointData& FJointData::operator*=(const float rhs)
{
	Position *= rhs;
	Velocity *= rhs;

	return *this;
}

