//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Components/TrajectoryErrorWarping.h"
#include "Utility/MotionMatchingUtils.h"

#define EPSILON 0.0001f

UTrajectoryErrorWarping::UTrajectoryErrorWarping()
	: WarpMode(ETrajectoryErrorWarpMode::Standard),
	  WarpRate(60.0f),
	  MinTrajectoryLength(0.75f),
	  ErrorActivationRange(FVector2D(0.25f, 30.0f)),
	  TrajectoryGenerator(nullptr)
{

}

void UTrajectoryErrorWarping::BeginPlay()
{
	Super::BeginPlay();

	OwningActor = GetOwner();

	TrajectoryGenerator = Cast<UTrajectoryGenerator_Base>(OwningActor->GetComponentByClass(UTrajectoryGenerator_Base::StaticClass()));

	if (TrajectoryGenerator == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("TrajectoryErrorWarp: could not find trajectory generator component."))
	}
}

void UTrajectoryErrorWarping::ApplyTrajectoryErrorWarping(const float DeltaTime, const float PlaybackSpeed /*=1.0f*/)
{
	if(TrajectoryGenerator == nullptr || OwningActor == nullptr)
		return;

	const float WarpAmount = CalculateTrajectoryErrorWarping(DeltaTime, PlaybackSpeed);

	OwningActor->AddActorLocalRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(WarpAmount)));
}

float UTrajectoryErrorWarping::CalculateTrajectoryErrorWarping(const float DeltaTime, const float PlaybackSpeed /*=1.0f*/) const
{
	if(!TrajectoryGenerator)
	{
		return 0.0f;
	}

	const TArray<FTrajectoryPoint>& TrajectoryPoints = TrajectoryGenerator->GetCurrentTrajectory().TrajectoryPoints;

	const int32 LastIndex = TrajectoryPoints.Num() - 1;

	if(LastIndex < 0 ||
		TrajectoryPoints[LastIndex].Position.SizeSquared() < MinTrajectoryLength * MinTrajectoryLength)
	{
		return 0.0f;
	}

	FVector DesiredDirection = FVector::ZeroVector;

	switch (WarpMode)
	{
		case ETrajectoryErrorWarpMode::Standard:
		{
			DesiredDirection = TrajectoryPoints[LastIndex].Position.GetSafeNormal();
		}
		break;
		case ETrajectoryErrorWarpMode::Strafe:
		{
			const float FacingAngle = FMath::DegreesToRadians(TrajectoryPoints[LastIndex].RotationZ);
			DesiredDirection = FVector(FMath::Sin(FacingAngle), 0.0f, FMath::Cos(FacingAngle)).GetSafeNormal() * -1.0f;
		}
		break;
	default: ;
	}

	const float LatErrorWarpAngle = FMath::RadiansToDegrees(DesiredDirection.HeadingAngle()) - 90.0f;

	float ReturnErrorWarp = 0.0f;
	if (FMath::Abs(LatErrorWarpAngle) < ErrorActivationRange.Y)
	{
		if (LatErrorWarpAngle < -ErrorActivationRange.X)
		{
			ReturnErrorWarp = FMath::Max(-WarpRate * DeltaTime * PlaybackSpeed, LatErrorWarpAngle);
		}
		else if (LatErrorWarpAngle > ErrorActivationRange.X)
		{
			ReturnErrorWarp = FMath::Min(WarpRate * DeltaTime * PlaybackSpeed, LatErrorWarpAngle);
		}
	}

	return ReturnErrorWarp;
}

void UTrajectoryErrorWarping::SetMode(const ETrajectoryErrorWarpMode InWarpMode, const float InWarpRate,
	const float InMinTrajectoryLength, const float MinActivation, const float MaxActivation)
{
	WarpMode = InWarpMode;
	WarpRate = InWarpRate;
	MinTrajectoryLength = InMinTrajectoryLength;
	ErrorActivationRange = FVector2D(MinActivation, MaxActivation);
}
