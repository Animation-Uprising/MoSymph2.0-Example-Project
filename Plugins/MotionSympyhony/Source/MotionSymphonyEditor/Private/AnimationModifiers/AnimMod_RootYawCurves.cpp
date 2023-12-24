//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimationModifiers/AnimMod_RootYawCurve.h"
#include "Animation/AnimSequence.h"

void UAnimMod_RootYawCurve::OnApply_Implementation(UAnimSequence* AnimationSequence)
{
	if (!AnimationSequence)
	{
		return;
	}
	
	const FName YawCurveName = FName(TEXT("RootVelocity_Yaw"));

	if (UAnimationBlueprintLibrary::DoesCurveExist(AnimationSequence,
		YawCurveName, ERawCurveTrackTypes::RCT_Float))
	{
		UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, YawCurveName, false);
	}

	UAnimationBlueprintLibrary::AddCurve(AnimationSequence, YawCurveName, ERawCurveTrackTypes::RCT_Float, false);

	//Speed Key Rate
	const float KeyRate = 1.0f / 30.0f; //Only do it at 30Hz to avoid unnecessary keys
	const float HalfKeyRate = KeyRate * 0.5f;

	for (float Time = 0.0f; Time < AnimationSequence->GetPlayLength(); Time += KeyRate)
	{
		const float KeyTime = Time + HalfKeyRate;
		FTransform RootMotion = AnimationSequence->ExtractRootMotion(Time, KeyRate, false);
		
		const float YawSpeed = RootMotion.Rotator().Yaw / KeyRate;
		
		UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, YawCurveName, KeyTime, YawSpeed);
	}
}

void UAnimMod_RootYawCurve::OnRevert_Implementation(UAnimSequence* AnimationSequence)
{
	if (!AnimationSequence)
	{
		return;
	}
	
	UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, FName(TEXT("RootVelocity_Yaw")), false);
}
