//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/TrajectoryGenerator_Base.h"
#include "TrajectoryErrorWarping.generated.h"

/** This enumeration holds the different modes for trajectory error warping 
when using motion matching with Motion Symphony*/
UENUM(BlueprintType)
enum class ETrajectoryErrorWarpMode : uint8 
{
	Disabled,
	Standard,
	Strafe
};

/** This actor component contains utility functionality for motion matching with Motion Symphony.
It is impossible to have all possible angles of animation movement (root motion) and so procedural animation 
warping is required to help conform root motion to gameplay. This is achieved in two ways, firstly, procedurally 
rotating the character on the Up-Axis over time so that it actually moves in the desired direction. Secondly it 
can be used alongside the 'Strider' plugin to warp the stride of the character to achieve varying speeds and 
movement angles.*/
UCLASS(BlueprintType, Category = "Motion Matching", meta = (BlueprintSpawnableComponent))
class MOTIONSYMPHONY_API UTrajectoryErrorWarping : public UActorComponent
{
	GENERATED_BODY()

public:
	/** The mode of trajectory error warping. It can be turned of, set to standard (follow your nose movement) or strafe*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	ETrajectoryErrorWarpMode WarpMode;

	/** The rate in 'degrees per second' that procedural rotation will be applied to achieve trajectory error warping*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = 0.0f))
	float WarpRate;

	/** The minimum length of the desired trajectory that is required for trajectory error warping to activate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (ClampMin = 0.0f))
	float MinTrajectoryLength;

	/** Trajectory Error Warping will only occur within the error activation range. This should usually be kept small 
	for realistic movement as it is better for an animation to turn the character if it can. Alternatively, this can
	be set high, along with WarpRate' to increase responsiveness of movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FVector2D ErrorActivationRange;

private:
	AActor* OwningActor;
	UTrajectoryGenerator_Base* TrajectoryGenerator;

public:
	UTrajectoryErrorWarping();

	virtual void BeginPlay() override;

	/** Calculates and applies trajectory error warping to the actor*/
	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|TrajectoryErrorWarping")
	void ApplyTrajectoryErrorWarping(const float DeltaTime, const float PlaybackSpeed = 1.0f);

	/** Calculates trajectory error warping, returning the rotation amount (Z-Axis rotation). This is
	intended for developers who wish to apply the trajectory error warping themselves.*/
	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|TrajectoryErrorWarping")
	float CalculateTrajectoryErrorWarping(const float DeltaTime, const float PlaybackSpeed = 1.0f) const;

	UFUNCTION(BlueprintCallable, Category = "MotionSymphony|TrajectoryErrorWarping")
	void SetMode(const ETrajectoryErrorWarpMode InWarpMode, const float InWarpRate, const float InMinTrajectoryLength,
		const float MinActivation, const float MaxActivation);
};