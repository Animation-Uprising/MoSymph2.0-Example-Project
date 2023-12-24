//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Components/DistanceMatching.h"
#include "DrawDebugHelpers.h"
#include "MatchFeatures/MatchFeature_Distance.h"
#include "Runtime/Launch/Resources/Version.h"

#define LOCTEXT_NAMESPACE "MotionSymphony"

static TAutoConsoleVariable<int32> CVarDistanceMatchingDebug(
	TEXT("a.AnimNode.MoSymph.DistanceMatch.Debug"),
	0,
	TEXT("Enables Debug Mode for Distance Matching. \n")
	TEXT("<=0: Off \n")
	TEXT("  1: On"));

UDistanceMatching::UDistanceMatching()
	: bAutomaticTriggers(false),
	DistanceTolerance(5.0f),
	MinPlantDetectionAngle(130.0f),
	MinPlantSpeed(100.0f),
	MinPlantAccel(100.0f),
	TriggeredTransition(EDistanceMatchTrigger::None),
	CurrentInstanceId(0),
	bDestinationReached(false),
	LastFrameAccelSqr(0.0f),
	DistanceToMarker(0.0f),
	TimeToMarker(0.0f),
	MarkerVector(FVector::ZeroVector),
	DistanceMatchType(EDistanceMatchType::None),
	DistanceMatchBasis(EDistanceMatchBasis::Positional),
	ParentActor(nullptr),
	MovementComponent(nullptr)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDistanceMatching::TriggerStart(float DeltaTime)
{
	if(!CalculateStartLocation(MarkerVector, DeltaTime, 50))
	{
		MarkerVector = ParentActor->GetActorLocation();
	}
	
	DistanceMatchType = EDistanceMatchType::Backward;
	DistanceMatchBasis = EDistanceMatchBasis::Positional;
	TriggeredTransition = EDistanceMatchTrigger::Start;

	LastPosition = ParentActor->GetActorLocation();
	DistanceToMarker = -FVector::Distance(LastPosition, MarkerVector);
	
	if ((++CurrentInstanceId) > 1000000)
	{
		CurrentInstanceId = 0;
	}
}

void UDistanceMatching::TriggerStop(float DeltaTime)
{
	if(CalculateStopLocation(MarkerVector, DeltaTime, 50))
	{
		DistanceMatchType = EDistanceMatchType::Forward;
		DistanceMatchBasis = EDistanceMatchBasis::Positional;
		TriggeredTransition = EDistanceMatchTrigger::Stop;
		bDestinationReached = false;

		LastPosition = ParentActor->GetActorLocation();
		DistanceToMarker = FVector::Distance(ParentActor->GetActorLocation(), MarkerVector);
	
		if ((++CurrentInstanceId) > 1000000)
		{
			CurrentInstanceId = 0;
		}
	}
}

void UDistanceMatching::TriggerPlant(float DeltaTime)
{
	if (CalculateStopLocation(MarkerVector, DeltaTime, 50))
	{
		DistanceMatchType = EDistanceMatchType::Both;
		DistanceMatchBasis = EDistanceMatchBasis::Positional;
		TriggeredTransition = EDistanceMatchTrigger::Plant;
		bDestinationReached = false;

		LastPosition = ParentActor->GetActorLocation();
		DistanceToMarker = FVector::Distance(ParentActor->GetActorLocation(), MarkerVector);

		if ((++CurrentInstanceId) > 1000000)
		{
			CurrentInstanceId = 0;
		}
	}
}

void UDistanceMatching::TriggerPivotFrom()
{
	DistanceMatchType = EDistanceMatchType::Backward;
	DistanceMatchBasis = EDistanceMatchBasis::Rotational;
	TriggeredTransition = EDistanceMatchTrigger::Pivot;

	MarkerVector = ParentActor->GetActorForwardVector();

	if ((++CurrentInstanceId) > 1000000)
	{
		CurrentInstanceId = 0;
	}
}

void UDistanceMatching::TriggerPivotTo()
{
	FVector Accel = MovementComponent->GetCurrentAcceleration();
	Accel.Z = 0.0f;
	if(Accel.SizeSquared() > 0.0001f)
	{
		MarkerVector = Accel.GetSafeNormal();

		DistanceMatchType = EDistanceMatchType::Forward;
		DistanceMatchBasis = EDistanceMatchBasis::Rotational;
		TriggeredTransition = EDistanceMatchTrigger::Pivot;
		bDestinationReached = false;

		if ((++CurrentInstanceId) > 1000000)
		{
			CurrentInstanceId = 0;
		}
	}
}

void UDistanceMatching::TriggerTurnInPlaceFrom()
{
	MarkerVector = ParentActor->GetActorForwardVector();

	DistanceMatchType = EDistanceMatchType::Backward;
	DistanceMatchBasis = EDistanceMatchBasis::Rotational;
	TriggeredTransition = EDistanceMatchTrigger::TurnInPlace;


	if ((++CurrentInstanceId) > 1000000)
	{
		CurrentInstanceId = 0;
	}
}

void UDistanceMatching::TriggerTurnInPlaceTo(FVector DesiredDirection)
{
	MarkerVector = DesiredDirection; //Could be sourced from Camera

	DistanceMatchType = EDistanceMatchType::Forward;
	DistanceMatchBasis = EDistanceMatchBasis::Rotational;
	TriggeredTransition = EDistanceMatchTrigger::TurnInPlace;


	if ((++CurrentInstanceId) > 1000000)
	{
		CurrentInstanceId = 0;
	}
}

void UDistanceMatching::TriggerJump(float DeltaTime)
{
	DistanceMatchType = EDistanceMatchType::Both;
	TriggeredTransition = EDistanceMatchTrigger::Jump;
	bDestinationReached = false;

	if ((++CurrentInstanceId) > 1000000)
	{
		CurrentInstanceId = 0;
	}
}

void UDistanceMatching::StopDistanceMatching()
{
	DistanceMatchType = EDistanceMatchType::None;
	TriggeredTransition = EDistanceMatchTrigger::None;
	bDestinationReached = false;
	DistanceToMarker = 0.0f;

	if ((++CurrentInstanceId) > 1000000)
	{
		CurrentInstanceId = 0;
	}
}

float UDistanceMatching::GetMarkerDistance()
{
	return DistanceToMarker;
}

void UDistanceMatching::DetectTransitions(float DeltaTime)
{
	if(!MovementComponent)
	{
		return;
	}

	FVector Velocity = MovementComponent->Velocity;
	FVector Acceleration = MovementComponent->GetCurrentAcceleration();
	const float SpeedSqr = Velocity.SizeSquared();
	const float AccelSqr = Acceleration.SizeSquared();

	//Detect Starts
	if(DistanceMatchType != EDistanceMatchType::Backward
		&& LastFrameAccelSqr < 0.001f && AccelSqr > 0.001f)
	{
		TriggerStart(DeltaTime);
		return;
	}
	else if(DistanceMatchType != EDistanceMatchType::Forward
		&& SpeedSqr > 0.001f && AccelSqr < 0.001f)
	{
		TriggerStop(DeltaTime);
		return;
	}
	else if(DistanceMatchType != EDistanceMatchType::Both)
	{
		//Detect plants
		//Can only plant if the speed is above a certain amount
		if(Velocity.SizeSquared() > MinPlantSpeed * MinPlantSpeed
			&& Acceleration.SizeSquared() > MinPlantAccel * MinPlantAccel)
		{
			Velocity.Normalize();
			Acceleration.Normalize();

			float Angle = FMath::RadiansToDegrees(acosf(FVector::DotProduct(Velocity, Acceleration)));

			if(FMath::Abs(Angle) > MinPlantDetectionAngle)
			{
				TriggerPlant(DeltaTime);
				return;
			}
		}
	}

	LastFrameAccelSqr = AccelSqr;
}

EDistanceMatchTrigger UDistanceMatching::GetAndConsumeTriggeredTransition()
{
	const EDistanceMatchTrigger ConsumedTrigger = TriggeredTransition;
	TriggeredTransition = EDistanceMatchTrigger::None;

	return ConsumedTrigger;
}

float UDistanceMatching::CalculateMarkerDistance() const
{
	if(!ParentActor || DistanceMatchType == EDistanceMatchType::None)
	{
		return 0.0f;
	}

	if(DistanceMatchBasis == EDistanceMatchBasis::Positional)
	{
		return FVector::Distance(ParentActor->GetActorLocation(), MarkerVector);
	}
	else
	{
		return FMath::RadiansToDegrees(acosf(FVector::DotProduct(ParentActor->GetActorForwardVector(), MarkerVector)));
	}
}

float UDistanceMatching::GetTimeToMarker() const
{
	return TimeToMarker;
}

EDistanceMatchType UDistanceMatching::GetDistanceMatchType() const
{
	return DistanceMatchType;
}

uint32 UDistanceMatching::GetCurrentInstanceId() const
{
	return CurrentInstanceId;
}

bool UDistanceMatching::DoesCurrentStateMatchFeature(UMatchFeature_Distance* DistanceMatchFeature) const
{
	if(DistanceMatchFeature->DistanceMatchType == DistanceMatchType
		&& DistanceMatchFeature->DistanceMatchBasis == DistanceMatchBasis
		&& DistanceMatchFeature->DistanceMatchTrigger == TriggeredTransition)
	{
		return true;
	}

	return false;
}

// Called when the game starts
void UDistanceMatching::BeginPlay()
{
	Super::BeginPlay();

	PrimaryComponentTick.bCanEverTick = bAutomaticTriggers;
	
	ParentActor = GetOwner();
	MovementComponent = Cast<UCharacterMovementComponent>(GetOwner()->GetComponentByClass(UCharacterMovementComponent::StaticClass()));

	if (MovementComponent == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("DistanceMatching: Cannot find Movement Component to predict motion. Disabling component."))

		PrimaryComponentTick.bCanEverTick = false;
		bAutomaticTriggers = false;
		return;
	}
}


// Called every frame
void UDistanceMatching::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//Currently only the walking mode is supported
	if(MovementComponent->MovementMode != EMovementMode::MOVE_Walking)
	{
		StopDistanceMatching();
		return;
	}
	
	if(bAutomaticTriggers)
	{
		DetectTransitions(DeltaTime);
	}

	if(DistanceMatchType == EDistanceMatchType::None)
	{
		return;
	}

	const FVector CharacterPosition = ParentActor->GetActorLocation();
	const float MoveDelta = FVector::Distance(CharacterPosition, LastPosition);
	switch(DistanceMatchType)
	{
		case EDistanceMatchType::Backward: 
		{
			DistanceToMarker -= MoveDelta;
		} 
		break;
		case EDistanceMatchType::Forward: 
		{
			DistanceToMarker -= MoveDelta;
			if(DistanceToMarker < DistanceTolerance)
			{
				DistanceToMarker = 0.0f;
				StopDistanceMatching();
			}
		}
		break;
		case EDistanceMatchType::Both:
		{
			DistanceToMarker -= MoveDelta;
			if(!bDestinationReached)
			{
				if (DistanceToMarker < DistanceTolerance)
				{
					DistanceToMarker = 0.0f;
					bDestinationReached = true;
				}
			}
		}
		default:
			{
				DistanceToMarker = 0.0f;
			}
		break;
	}

	LastPosition = CharacterPosition;

#if ENABLE_ANIM_DEBUG && ENABLE_DRAW_DEBUG
	const int32 DebugLevel = CVarDistanceMatchingDebug.GetValueOnGameThread();

	if (DebugLevel == 1 && DistanceMatchType != EDistanceMatchType::None)
	{
		FColor DebugColor = FColor::Green;
		switch (DistanceMatchType)
		{
			case EDistanceMatchType::Backward: DebugColor = FColor::Blue; break;
			case EDistanceMatchType::Forward: DebugColor = FColor::Green; break;
			case EDistanceMatchType::Both: DebugColor = FColor::Purple; break;
			default: DebugColor = FColor::Blue; break;
		}

		UWorld* World = ParentActor->GetWorld();

		if(World)
		{
			DrawDebugSphere(World, MarkerVector, 10, 16, DebugColor, false, -1.0f, 0, 0.5f);
		}
	}
#endif
}



FDistanceMatchingModule::FDistanceMatchingModule()
	: AnimSequence(nullptr),
	LastKeyChecked(0),
	MaxDistance(0.0f)
{

}

void FDistanceMatchingModule::Setup(UAnimSequenceBase* InAnimSequence, const FName& DistanceCurveName)
{
	AnimSequence = InAnimSequence;

	if(!AnimSequence)
	{
		return;
	}

	
#if ENGINE_MINOR_VERSION > 2	
	const FRawCurveTracks& RawCurves = InAnimSequence->GetCurveData();
	if(const FFloatCurve* DistanceCurve = static_cast<const FFloatCurve*>(RawCurves.GetCurveData(DistanceCurveName)))
	{
		CurveKeys = DistanceCurve->FloatCurve.GetCopyOfKeys();

		for (const FRichCurveKey& Key : CurveKeys)
		{
			if (FMath::Abs(Key.Value) > MaxDistance)
			{
				MaxDistance = FMath::Abs(Key.Value);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Distance matching curve could not be found. Distance matching node will not operate as expected."));
	}
#else
	FSmartName CurveName;
	InAnimSequence->GetSkeleton()->GetSmartNameByName(USkeleton::AnimCurveMappingName, DistanceCurveName, CurveName);
	if (CurveName.IsValid())
	{
		const FRawCurveTracks& RawCurves = InAnimSequence->GetCurveData();
		if(const FFloatCurve* DistanceCurve = static_cast<const FFloatCurve*>(RawCurves.GetCurveData(CurveName.UID)))
		{
			CurveKeys = DistanceCurve->FloatCurve.GetCopyOfKeys();

			for (const FRichCurveKey& Key : CurveKeys)
			{
				if (FMath::Abs(Key.Value) > MaxDistance)
				{
					MaxDistance = FMath::Abs(Key.Value);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Distance matching curve could not be found. Distance matching node will not operate as expected."));
		}
	}
#endif
}

void FDistanceMatchingModule::Initialize()
{
	LastKeyChecked = 0;
}

float FDistanceMatchingModule::FindMatchingTime(float DesiredDistance, bool bNegateCurve)
{
	if(CurveKeys.Num() < 2
	|| FMath::Abs(DesiredDistance) > FMath::Abs(MaxDistance))
	{
		return -1.0f;
	}

	//Find the time in the animation with the matching distance
	LastKeyChecked = FMath::Clamp(LastKeyChecked, 0, CurveKeys.Num() - 1);
	FRichCurveKey* PKey = &CurveKeys[LastKeyChecked];
	FRichCurveKey* SKey = nullptr;

	const float Negator = bNegateCurve ? -1.0f : 1.0f;

	for (int32 i = LastKeyChecked; i < CurveKeys.Num(); ++i)
	{
		FRichCurveKey& Key = CurveKeys[i];

		if (Key.Value * Negator > DesiredDistance)
		{
			PKey = &Key;
			//LastKeyChecked = i;
		}
		else
		{
			SKey = &Key;
			break;
		}
	}

	if (!SKey)
	{
		return PKey->Time;
	}

	const float DV = (SKey->Value * Negator) - (PKey->Value * Negator);

	if(DV < 0.000001f)
	{
		return PKey->Time;
	}

	const float DT = SKey->Time - PKey->Time;

	return ((DT / DV) * (DesiredDistance - (PKey->Value * Negator))) + PKey->Time;
}

bool UDistanceMatching::CalculateStartLocation(FVector& OutStartLocation, const float DeltaTime, const int32 MaxIterations) const
{
	
	const FVector TargetVelocity = MovementComponent->Velocity;
	const FVector Acceleration = TargetVelocity.GetSafeNormal() * MovementComponent->GetMaxAcceleration();
	const float Friction = FMath::Max(MovementComponent->GroundFriction, 0.0f);

	const float MIN_TICK_TIME = 1e-6;
	if (DeltaTime < MIN_TICK_TIME)
	{
		return false;
	}
	
	FVector LastVelocity = FVector::ZeroVector;
	
	FVector CurrentLocation = ParentActor->GetActorLocation();
	int32 Iterations = 0;
	while (Iterations < MaxIterations)
	{
		++Iterations;
		
		FVector TotalAcceleration = Acceleration;
		TotalAcceleration.Z = 0.0f;

		// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
		const FVector AccelDir = TotalAcceleration.GetSafeNormal();
		const float VelSize = LastVelocity.Size();
		TotalAcceleration += -(LastVelocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.0f);
		// Apply acceleration
		LastVelocity += TotalAcceleration * DeltaTime;

		CurrentLocation -= LastVelocity * DeltaTime;

		if(TargetVelocity.SizeSquared() - LastVelocity.SizeSquared() < 0.1)
		{
			//Target Velocity reached
			OutStartLocation = CurrentLocation;
			break;
		}

	}

	return true;
}

bool UDistanceMatching::CalculateStopLocation(FVector& OutStopLocation, const float DeltaTime, const int32 MaxIterations)
{
	const FVector CurrentLocation = ParentActor->GetActorLocation();
	const FVector Velocity = MovementComponent->Velocity;
	const FVector Acceleration = MovementComponent->GetCurrentAcceleration();
	float Friction = MovementComponent->GroundFriction * MovementComponent->BrakingFrictionFactor;
	float BrakingDeceleration = MovementComponent->BrakingDecelerationWalking;

	const float MIN_TICK_TIME = 1e-6;
	if (DeltaTime < MIN_TICK_TIME)
	{
		return false;
	}
	
	const bool bZeroAcceleration = Acceleration.IsZero();

	if ((Acceleration | Velocity) > 0.0f)
	{
		return false;
	}
	
	BrakingDeceleration = FMath::Max(BrakingDeceleration, 0.0f);
	Friction = FMath::Max(Friction, 0.0f);
	const bool bZeroFriction = (Friction < 0.00001f);
	const bool bZeroBraking = (BrakingDeceleration == 0.00001f);

	//Won't stop if there is no Braking acceleration or friction
	if (bZeroAcceleration && bZeroFriction && bZeroBraking)
	{
		return false;
	}

	FVector LastVelocity = bZeroAcceleration ? Velocity : Velocity.ProjectOnToNormal(Acceleration.GetSafeNormal());
	LastVelocity.Z = 0;

	FVector LastLocation = CurrentLocation;

	int Iteration = 0;
	float PredictionTime = 0.0f;
	while (Iteration < MaxIterations)
	{
		++Iteration;

		const FVector OldVel = LastVelocity;

		// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
		if (bZeroAcceleration)
		{
			// subdivide braking to get reasonably consistent results at lower frame rates
			// (important for packet loss situations w/ networking)
			float RemainingTime = DeltaTime;
			const float MaxDeltaTime = (1.0f / 33.0f);

			// Decelerate to brake to a stop
			const FVector BrakeDecel = (bZeroBraking ? FVector::ZeroVector : (-BrakingDeceleration * LastVelocity.GetSafeNormal()));
			while (RemainingTime >= MIN_TICK_TIME)
			{
				// Zero friction uses constant deceleration, so no need for iteration.
				const float DT = ((RemainingTime > MaxDeltaTime && !bZeroFriction) ? FMath::Min(MaxDeltaTime, RemainingTime * 0.5f) : RemainingTime);
				RemainingTime -= DT;
				
				// apply friction and braking
				LastVelocity = LastVelocity + ((-Friction) * LastVelocity + BrakeDecel) * DT;

				// Don't reverse direction
				if ((LastVelocity | OldVel) <= 0.f)
				{
					LastVelocity = FVector::ZeroVector;
					break;
				}
			}

			// Clamp to zero if nearly zero, or if below min threshold and braking.
			const float VSizeSq = LastVelocity.SizeSquared();
			if (VSizeSq <= 1.f || (!bZeroBraking && VSizeSq <= FMath::Square(10)))
			{
				LastVelocity = FVector::ZeroVector;
			}
		}
		else
		{
			FVector TotalAcceleration = Acceleration;
			TotalAcceleration.Z = 0;

			// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
			const FVector AccelDir = TotalAcceleration.GetSafeNormal();
			const float VelSize = LastVelocity.Size();
			TotalAcceleration += -(LastVelocity - AccelDir * VelSize) * Friction;
			// Apply acceleration
			LastVelocity += TotalAcceleration * DeltaTime;
		}

		LastLocation += LastVelocity * DeltaTime;

		PredictionTime += DeltaTime;

		// Clamp to zero if nearly zero, or if below min threshold and braking.
		const float VSizeSq = LastVelocity.SizeSquared();
		if (VSizeSq <= 1.f
			|| (LastVelocity | OldVel) <= 0.f)
		{
			TimeToMarker = PredictionTime;
			OutStopLocation = LastLocation;
			return true;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE