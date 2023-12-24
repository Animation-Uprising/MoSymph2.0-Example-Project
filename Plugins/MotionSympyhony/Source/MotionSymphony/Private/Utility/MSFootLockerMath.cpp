//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MSFootLockerMath.h"

float UMSFootLockerMath::AngleBetween(const FVector& A, const FVector& B)
{
	return FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(A, B)));
}


float UMSFootLockerMath::GetPointOnPlane(const FVector& Point, const FVector & SlopeNormal, const FVector & SlopeLocation)
{
	//z = (-a(X - X1) - b(y - y1)) / c) + z1
	return ((-(SlopeNormal.X * (Point.X - SlopeLocation.X))
			- (SlopeNormal.Y * (Point.Y - SlopeLocation.Y))) / SlopeNormal.Z)
			+ SlopeLocation.Z;
}

FVector UMSFootLockerMath::GetBoneWorldLocation(const FTransform& InBoneTransform_CS, FAnimInstanceProxy* InAnimInstanceProxy)
{
	return InAnimInstanceProxy->GetComponentTransform().TransformPosition(InBoneTransform_CS.GetLocation());
}

FVector UMSFootLockerMath::GetBoneWorldLocation(const FVector& InBoneLocation_CS, FAnimInstanceProxy* InAnimInstanceProxy)
{
	return InAnimInstanceProxy->GetComponentTransform().TransformPosition(InBoneLocation_CS);
}