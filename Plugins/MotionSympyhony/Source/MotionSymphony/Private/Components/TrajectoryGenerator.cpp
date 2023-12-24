// Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "TrajectoryGenerator.h"
#include "Camera/CameraComponent.h"
#include "Data/InputProfile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Utility/MotionMatchingUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "AIController.h"
#include "Quaternion.h"
#include "WorldPartition/RuntimeSpatialHash/RuntimeSpatialHashGridHelper.h"

#define EPSILON 0.0001f

UTrajectoryGenerator::UTrajectoryGenerator()
	: StrafeDirection(FVector(0.0f)),
	  MaxSpeed(400.0f), 
	  MoveResponse(15.0f), 
	  TurnResponse(15.0f), 
	  bResetDirectionOnIdle(true),
      TrajectoryModel(ETrajectoryModel::Spring),
	  TrajectoryBehaviour(ETrajectoryMoveMode::Standard),
	  TrajectoryControlMode(ETrajectoryControlMode::PlayerControlled),
	  LastDesiredOrientation(0.0f),
      MoveResponse_Remapped(15.0f),
	  TurnResponse_Remapped(15.0f)
{
}

void UTrajectoryGenerator::UpdatePrediction(float DeltaTime)
{
	if(TrajectoryControlMode == ETrajectoryControlMode::AIControlled
		&& !bUsePathAsTrajectoryForAI)
	{
		CalculateInputVectorFromAINavAgent();
	}
	
	FVector DesiredLinearVelocity;
	CalculateDesiredLinearVelocity(DesiredLinearVelocity);
	const FVector DesiredLinearDisplacement = DesiredLinearVelocity / FMath::Max(EPSILON, SampleRate);
	
	if(TrajectoryControlMode == ETrajectoryControlMode::AIControlled
		&& bUsePathAsTrajectoryForAI)
	{
		PathFollowPrediction(DeltaTime, TrajectoryIterations, DesiredLinearDisplacement);
	}
	else
	{
		switch(TrajectoryModel)
		{
		case ETrajectoryModel::Spring:
			{
				InputPrediction(DeltaTime, DesiredLinearDisplacement);
			} break;
		case ETrajectoryModel::UECharacterMovement:
			{
				CapsulePrediction(DeltaTime);
			} break;
		}
	}

	Super::UpdatePrediction(DeltaTime); //Need this for debug drawing
}

void UTrajectoryGenerator::PathFollowPrediction(const float DeltaTime, const int32 Iterations, const FVector& DesiredLinearDisplacement)
{
	const FVector RefLocation = OwningActor->GetActorLocation();
	if(APawn* Pawn = Cast<APawn>(OwningActor))
	{
		if(AAIController* Controller = Pawn->GetController<AAIController>())
		{
			if(UPathFollowingComponent* PathComponent = Controller->GetPathFollowingComponent())
			{
				if(const FNavigationPath* PathPtr = PathComponent->GetPath().Get())
				{
					const TArray<FNavPathPoint>& Points = PathPtr->GetPathPoints();
					
					if(Points.Num() > 0)
					{
						const float Step = DesiredLinearDisplacement.Size();
						float Overflow = Step;

						const int32 PathStartIndex = PathComponent->GetCurrentPathIndex() + 1;
						int32 TrajectoryPointIndex = 1;
						bool bTrajectoryEnded = Step <= 0;
						for(int32 PathPointIndex = PathStartIndex; !bTrajectoryEnded && PathPointIndex < Points.Num(); ++PathPointIndex)
						{
							FVector StartLocation = Points[PathPointIndex - 1].Location;
							FVector EndLocation = Points[PathPointIndex].Location;
							if(PathPointIndex == PathStartIndex)
							{
								StartLocation = FMath::ClosestPointOnLine(StartLocation, EndLocation, RefLocation);
							}

							const FVector Segment = EndLocation - StartLocation;
							const FVector SegmentDir = Segment.GetSafeNormal();
							const float Yaw = SegmentDir.Rotation().Yaw;
							const float SegmentLength = Segment.Size();

							float Length = Overflow;
							Overflow = 0.0f;
							FVector Ref = StartLocation - RefLocation;
							Ref.Z = 0.0f;
							while(!bTrajectoryEnded&& Length < SegmentLength)
							{
								TrajPositions[TrajectoryPointIndex] = Ref + SegmentDir * Length;
								TrajPositions[TrajectoryPointIndex].Z = 0;
								TrajRotations[TrajectoryPointIndex] = Yaw;
								Length += Step;

								bTrajectoryEnded = ++TrajectoryPointIndex == Iterations;
							}
							Overflow = Length - SegmentLength;
						}
						if(!bTrajectoryEnded)
						{
							while(TrajectoryPointIndex < Iterations)
							{
								TrajPositions[TrajectoryPointIndex] = TrajPositions[TrajectoryPointIndex - 1];
								TrajRotations[TrajectoryPointIndex] = TrajRotations[TrajectoryPointIndex - 1];
								++TrajectoryPointIndex;
							}
						}
						
						TrajRotations[0] = OwningActor->GetActorRotation().Yaw;
					}
				}
			}
		}
	}
}

void UTrajectoryGenerator::InputPrediction(const float DeltaTime, const FVector& DesiredLinearDisplacement)
{
	if(NewTrajPosition.Num() == 0)
	{
		return;
	}
	
	float DesiredOrientation;
	if (TrajectoryBehaviour != ETrajectoryMoveMode::Standard)
	{
		DesiredOrientation = FMath::RadiansToDegrees(FMath::Atan2(StrafeDirection.Y, StrafeDirection.X));
	}
	else if (DesiredLinearDisplacement.SizeSquared() > EPSILON)
	{
		DesiredOrientation = FMath::RadiansToDegrees(FMath::Atan2(
			DesiredLinearDisplacement.Y, DesiredLinearDisplacement.X));
	}
	else
	{
		if(bResetDirectionOnIdle)
		{
			const USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(
			OwningActor->GetComponentByClass(USkeletalMeshComponent::StaticClass()));
			
			DesiredOrientation = SkelMesh->GetComponentToWorld().Rotator().Yaw + CharacterFacingOffset;
		}
		else
		{
			DesiredOrientation = LastDesiredOrientation;
		}
	}

	LastDesiredOrientation = DesiredOrientation;

	NewTrajPosition[0] = FVector::ZeroVector;
	TrajRotations[0] = 0.0f;

	const int32 Iterations = TrajPositions.Num();
	float CumRotation = 0.0f;
	
	for (int32 i = 1; i < Iterations; ++i)
	{
		const float Percentage = static_cast<float>(i) / FMath::Max(1.0f, static_cast<float>(Iterations - 1));
		FVector TrajDisplacement = TrajPositions[i] - TrajPositions[i-1];

		FVector AdjustedTrajDisplacement = FMath::Lerp(TrajDisplacement, DesiredLinearDisplacement,
			1.0f - FMath::Exp((-MoveResponse_Remapped * DeltaTime) * Percentage));

		NewTrajPosition[i] = NewTrajPosition[i - 1] + AdjustedTrajDisplacement;

		//TrajRotations[i] = DesiredOrientation;

		TrajRotations[i] = FMath::RadiansToDegrees(FMotionMatchingUtils::LerpAngle(
			FMath::DegreesToRadians(TrajRotations[i]),
			FMath::DegreesToRadians(DesiredOrientation) ,
			1.0f - FMath::Exp((-TurnResponse_Remapped * DeltaTime) * Percentage)));
	}

	for (int32 i = 0; i < Iterations; ++i)
	{
		TrajPositions[i] = NewTrajPosition[i];
	}
}

void UTrajectoryGenerator::CapsulePrediction(const float DeltaTime)
{
	//Movement
	const FVector CurrentLocation = OwningActor->GetActorLocation();
	FVector Velocity = CharacterMovement->Velocity;
	const FVector Acceleration = CharacterMovement->GetCurrentAcceleration();
	const bool bZeroAcceleration = Acceleration.IsZero();
	float Friction = CharacterMovement->GroundFriction;
	const float BrakingDeceleration = CharacterMovement->BrakingDecelerationWalking;

	//Rotation
	FRotator CurrentRotation = OwningActor->GetActorRotation();
	FRotator DeltaRot = CharacterMovement->GetDeltaRotation(DeltaTime);
	FRotator DesiredRotation = CharacterMovement->ComputeOrientToMovementRotation(CurrentRotation, 1.0f / SampleRate, DeltaRot);
	DesiredRotation.Pitch = 0.0f;
	DesiredRotation.Roll = 0.0f;
	DesiredRotation.Yaw = FRotator::NormalizeAxis(DesiredRotation.Yaw);
	const float AngleTolerance = 1e-3f;
	
	if(!HasMoveInput())
	{
		Friction = CharacterMovement->BrakingFriction * CharacterMovement->BrakingFrictionFactor;
	}

	Friction = FMath::Max(Friction, 0.0f);
	const bool bZeroFriction = (Friction < 0.00001f);
	const bool bZeroBraking = (BrakingDeceleration < 0.00001f);
	
	if(bZeroAcceleration
		&& bZeroFriction
		&& bZeroBraking)
	{
		return;
	}
	
	FVector LastLocation = CurrentLocation;
	float LastYaw = CurrentRotation.Yaw;
	TrajPositions[0] = FVector::ZeroVector;
	TrajRotations[0] = 0.0f;

	FVector AccelDir = Acceleration.GetSafeNormal();
	AccelDir.Z = 0.0f;
	Velocity.Z = 0.0f;
	constexpr float MaxDeltaTime = 1.0f / 33.0f;
	for(int32 TrajectoryIndex = 1; TrajectoryIndex < TrajectoryIterations; ++TrajectoryIndex)
	{
		const FVector OldVel = Velocity;
		const FVector BrakeDeceleration (bZeroBraking ? FVector::ZeroVector : (-BrakingDeceleration * Velocity.GetSafeNormal()));
		
		float RemainingTime = 1.0 / SampleRate;
		constexpr float MIN_TICK_TIME = 1e-6;
		while(RemainingTime >= MIN_TICK_TIME)
		{
			const float DT = RemainingTime > MaxDeltaTime ? MaxDeltaTime : RemainingTime;
			RemainingTime -= DT;
			
			if(bZeroAcceleration)
			{
				// apply friction and braking
				Velocity = Velocity + ((-Friction) * Velocity + BrakeDeceleration) * DT;

				//Don't reverse Direction
				  if((Velocity | OldVel) <= 0.0f)
				  {
				  	Velocity = FVector::ZeroVector;
				  	TrajPositions[TrajectoryIndex] = LastLocation - CurrentLocation;
				  	break;
				  }
			}
			else
			{
				// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
				const float VelSize = Velocity.Size();
				Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DT * Friction, 1.0f);

				//Apply Acceleration
				Velocity += Acceleration * DT;
				Velocity = Velocity.GetClampedToMaxSize(CharacterMovement->GetMaxSpeed());
			}
		}

		// Clamp to zero if nearly zero, or if below min threshold and braking.
		const float VSizeSq = Velocity.SizeSquared();
		if(VSizeSq <= UE_KINDA_SMALL_NUMBER || (!bZeroBraking && VSizeSq <= FMath::Square(10.0f)))
		{
			Velocity = FVector::ZeroVector;
		}
		
		LastLocation += Velocity * (1.0f / SampleRate);

		TrajPositions[TrajectoryIndex] = LastLocation - CurrentLocation;

		//Rotation
		if(CharacterMovement->bOrientRotationToMovement)
		{
			if(!FMath::IsNearlyEqual(LastYaw, DesiredRotation.Yaw, AngleTolerance))
			{
				CurrentRotation.Yaw = FMath::FixedTurn(LastYaw, DesiredRotation.Yaw, DeltaRot.Yaw);
			}
			LastYaw = CurrentRotation.Yaw;

			TrajRotations[TrajectoryIndex] = CurrentRotation.Yaw;
		}
	}
}

void UTrajectoryGenerator::Setup(TArray<float>& InTrajTimes)
{
	CharacterMovement = Cast<UCharacterMovementComponent>(
		OwningActor->GetComponentByClass(UCharacterMovementComponent::StaticClass()));

	NewTrajPosition.Empty(TrajectoryIterations);

	FVector ActorPosition = OwningActor->GetActorLocation();
	for (int32 i = 0; i < TrajectoryIterations; ++i)
	{
		NewTrajPosition.Emplace(ActorPosition);
	}
}

bool UTrajectoryGenerator::IsValidToUpdatePrediction()
{
	return NewTrajPosition.Num() != 0 && Super::IsValidToUpdatePrediction();
}

void UTrajectoryGenerator::CalculateDesiredLinearVelocity(FVector & OutVelocity)
{
	MoveResponse_Remapped = MoveResponse;
	TurnResponse_Remapped = TurnResponse;

	float SpeedDecayMultiplier = 1.0f;
	if (InputProfile != nullptr)
	{
		const FInputSet* InputSet = InputProfile->GetInputSet(InputVector);

		if (InputSet != nullptr)
		{
			InputVector.Normalize();

			if(bUseTurnDecay && !InputVector.IsNearlyZero())
			{
				AActor* Owner = GetOwner();
				const float InputDot = FVector::DotProduct(InputVector, Owner->GetActorTransform().GetUnitAxis(EAxis::Y));
				SpeedDecayMultiplier = FMath::GetMappedRangeValueClamped(FFloatRange(-1.0f, 1.0f),
					FFloatRange(0.5f, 1.0f), InputDot);

				UE_LOG(LogTemp, Warning, TEXT("InputDOT: %f, DecayMul: %f"), InputDot, SpeedDecayMultiplier);
			}

			InputVector *= InputSet->SpeedMultiplier;
			

			MoveResponse_Remapped = MoveResponse * InputSet->MoveResponseMultiplier;
			TurnResponse_Remapped = TurnResponse * InputSet->TurnResponseMultiplier;
		}
	}

	if(InputVector.SizeSquared() > 1.0f)
	{
		InputVector.Normalize();
	}

	OutVelocity = InputVector * MaxSpeed * SpeedDecayMultiplier;
}

void UTrajectoryGenerator::CalculateInputVectorFromAINavAgent()
{
	if(!CharacterMovement)
	{
		TrajectoryControlMode = ETrajectoryControlMode::PlayerControlled;
		return;
	}
	
	if(CharacterMovement->UseAccelerationForPathFollowing())
	{
		InputVector = CharacterMovement->GetCurrentAcceleration();
	}
	else
	{
		InputVector = CharacterMovement->RequestedVelocity;
	}
		
	InputVector.Normalize();
}

#if WITH_EDITORONLY_DATA
void UTrajectoryGenerator::DebugDrawTrajectory(const float InDeltaTime)
{
	Super::DebugDrawTrajectory(InDeltaTime);
	
	const FVector RefLocation = OwningActor->GetActorLocation();
	for(int32 i = 0; i < TrajectoryIterations; ++i)
	{
		DrawDebugCoordinateSystem(GetWorld(), RefLocation + TrajPositions[i],
			FRotator(0.0f, TrajRotations[i], 0.0f), 10, false, InDeltaTime * 1.2f);		
	}
}
#endif

void UTrajectoryGenerator::SetStrafeDirectionFromCamera(UCameraComponent* Camera)
{
	if (!Camera)
	{
		return;
	}

	StrafeDirection = Camera->GetForwardVector();
	StrafeDirection.Z = 0.0f;

	StrafeDirection.Normalize();
}
