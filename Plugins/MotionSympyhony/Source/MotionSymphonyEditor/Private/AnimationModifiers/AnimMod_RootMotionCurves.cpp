//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimationModifiers/AnimMod_RootMotionCurves.h"
#include "Animation/AnimSequence.h"

void UAnimMod_RootMotionCurves::OnApply_Implementation(UAnimSequence* AnimationSequence)
{
	if (!AnimationSequence)
	{
		return;
	}

	const FName MoveXCurveName = FName(TEXT("RootVelocity_X"));
	const FName MoveYCurveName = FName(TEXT("RootVelocity_Y"));
	const FName MoveZCurveName = FName(TEXT("RootVelocity_Z"));
	const FName YawCurveName = FName(TEXT("RootVelocity_Yaw"));

	if (UAnimationBlueprintLibrary::DoesCurveExist(AnimationSequence,
		MoveXCurveName, ERawCurveTrackTypes::RCT_Float))
	{
		UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, MoveXCurveName, false);
	}

	if (UAnimationBlueprintLibrary::DoesCurveExist(AnimationSequence,
		MoveYCurveName, ERawCurveTrackTypes::RCT_Float))
	{
		UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, MoveYCurveName, false);
	}

	if (UAnimationBlueprintLibrary::DoesCurveExist(AnimationSequence,
		MoveXCurveName, ERawCurveTrackTypes::RCT_Float))
	{
		UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, MoveZCurveName, false);
	}

	if (UAnimationBlueprintLibrary::DoesCurveExist(AnimationSequence,
		YawCurveName, ERawCurveTrackTypes::RCT_Float))
	{
		UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, YawCurveName, false);
	}

	UAnimationBlueprintLibrary::AddCurve(AnimationSequence, MoveXCurveName, ERawCurveTrackTypes::RCT_Float, false);
	UAnimationBlueprintLibrary::AddCurve(AnimationSequence, MoveYCurveName, ERawCurveTrackTypes::RCT_Float, false);
	UAnimationBlueprintLibrary::AddCurve(AnimationSequence, MoveZCurveName, ERawCurveTrackTypes::RCT_Float, false);
	UAnimationBlueprintLibrary::AddCurve(AnimationSequence, YawCurveName, ERawCurveTrackTypes::RCT_Float, false);

	//Speed Key Rate
	const float KeyRate = 1.0f / 30.0f; //Only do it at 30Hz to avoid unnecessary keys
	const float HalfKeyRate = KeyRate * 0.5f;

	for (float Time = 0.0f; Time < AnimationSequence->GetPlayLength(); Time += KeyRate)
	{
		const float KeyTime = Time + HalfKeyRate;
		FTransform RootMotion = AnimationSequence->ExtractRootMotion(Time, KeyRate, false);

		const FVector MoveVelocity = RootMotion.GetLocation() / KeyRate;
		const float YawSpeed = RootMotion.Rotator().Yaw / KeyRate;

		UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, MoveXCurveName, KeyTime, MoveVelocity.X);
		UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, MoveYCurveName, KeyTime, MoveVelocity.Y);
		UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, MoveZCurveName, KeyTime, MoveVelocity.Z);
		UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, YawCurveName, KeyTime, YawSpeed);
	}
}

void UAnimMod_RootMotionCurves::OnRevert_Implementation(UAnimSequence* AnimationSequence)
{
	if (!AnimationSequence)
	{
		return;
	}

	UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, FName(TEXT("RootVelocity_X")), false);
	UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, FName(TEXT("RootVelocity_Y")), false);
	UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, FName(TEXT("RootVelocity_Z")), false);
	UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, FName(TEXT("RootVelocity_Yaw")), false);
}
