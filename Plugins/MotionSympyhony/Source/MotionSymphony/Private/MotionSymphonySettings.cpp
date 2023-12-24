//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MotionSymphonySettings.h"

UMotionSymphonySettings::UMotionSymphonySettings(const FObjectInitializer& ObjectInitializer)
	: DebugScale_Velocity(1.0f),
	DebugScale_Point(1.0f),
	DebugColor_Trajectory(FColor::Red),
	DebugColor_TrajectoryPast(FColor(128, 0, 0)),
	DebugColor_DesiredTrajectory(FColor::Green),
	DebugColor_DesiredTrajectoryPast(FColor(0, 128, 0)),
	DebugColor_BodyVelocity(FColor::Orange),
	DebugColor_BodyAngularVelocity(FColor::Magenta),
	DebugColor_Joint(FColor::Blue),
	DebugColor_JointHeight(FColor::Yellow),
	DebugColor_JointFacing(FColor::Green),
	DebugColor_Custom1(FColor::Cyan),
	DebugColor_Custom2(FColor::Purple)
{
}
