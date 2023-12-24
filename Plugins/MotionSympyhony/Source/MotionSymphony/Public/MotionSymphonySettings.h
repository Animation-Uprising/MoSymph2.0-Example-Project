//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MotionSymphonySettings.generated.h"

UCLASS(config = Game, defaultconfig)
class MOTIONSYMPHONY_API UMotionSymphonySettings : public UObject
{
	GENERATED_BODY()

public:
	UMotionSymphonySettings(const FObjectInitializer& ObjectInitializer);
	
	/** The scale of velocity vectors in debug visualisation */
	UPROPERTY(EditAnywhere, config, Category = "Debug|Scale")
	float DebugScale_Velocity;

	/** The scale of point spheres in debug visualisation */
	UPROPERTY(EditAnywhere, config, Category = "Debug|Scale")
	float DebugScale_Point;

	/** The color of the trajectory debug visualisation */
	UPROPERTY(EditAnywhere, config, Category = "Debug|Colors")
	FColor DebugColor_Trajectory;

	UPROPERTY(EditAnywhere, config, Category = "Debug|Colors")
	FColor DebugColor_TrajectoryPast;

	/** The color of the trajectory debug visualisation */
	UPROPERTY(EditAnywhere, config, Category = "Debug|Colors")
	FColor DebugColor_DesiredTrajectory;

	UPROPERTY(EditAnywhere, config, Category = "Debug|Colors")
	FColor DebugColor_DesiredTrajectoryPast;

	/** The color of the body velocity debug visualisation */
	UPROPERTY(EditAnywhere, config, Category = "Debug|Colors")
	FColor DebugColor_BodyVelocity;

	/** The color of the angular body velocity debug visualisation */
	UPROPERTY(EditAnywhere, config, Category = "Debug|Colors")
	FColor DebugColor_BodyAngularVelocity;

	/** The color of the joint debug visualisation for both position and velocity */
	UPROPERTY(EditAnywhere, config, Category = "Debug|Colors")
	FColor DebugColor_Joint;

	/** The color of the joint height debug visualisation */
	UPROPERTY(EditAnywhere, config, Category = "Debug|Colors")
	FColor DebugColor_JointHeight;

	/** The color of the joint facing debug visualisation */
	UPROPERTY(EditAnywhere, config, Category = "Debug|Colors")
	FColor DebugColor_JointFacing;

	/** The color of custom debug visualisation (1)*/
	UPROPERTY(EditAnywhere, config, Category = "Debug|Colors")
	FColor DebugColor_Custom1;

	/** The color of custom debug visualisation (2)*/
	UPROPERTY(EditAnywhere, config, Category = "Debug|Colors")
	FColor DebugColor_Custom2;
};

