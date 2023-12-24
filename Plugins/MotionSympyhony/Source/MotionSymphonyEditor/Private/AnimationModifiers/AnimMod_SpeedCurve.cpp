//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimationModifiers/AnimMod_SpeedCurve.h"
#include "AnimationBlueprintLibrary.h"
#include "Animation/AnimSequence.h"

void UAnimMod_SpeedCurve::OnApply_Implementation(UAnimSequence* AnimationSequence)
{
	if (!AnimationSequence)
	{
		return;
	}

	FName CurveName = FName(TEXT("Speed"));

	bool bCurveExists = UAnimationBlueprintLibrary::DoesCurveExist(AnimationSequence, 
		CurveName, ERawCurveTrackTypes::RCT_Float);

	if (bCurveExists)
	{
		UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, CurveName, false);
	}

	UAnimationBlueprintLibrary::AddCurve(AnimationSequence, CurveName, ERawCurveTrackTypes::RCT_Float, false);

	//Speed Key Rate
	float KeyRate = 1.0f / 30.0f; //Only do it at 30Hz to avoid unnecessary keys
	float HalfKeyRate = KeyRate * 0.5f;

	for (float Time = 0.0f; Time < AnimationSequence->GetPlayLength(); Time += KeyRate)
	{
		float KeyTime = Time + HalfKeyRate;
		FVector MoveDelta = AnimationSequence->ExtractRootMotion(Time, KeyRate, false).GetLocation();

		float Speed = MoveDelta.Size() / KeyRate;

		UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, CurveName, KeyTime, Speed);
	}
}

void UAnimMod_SpeedCurve::OnRevert_Implementation(UAnimSequence* AnimationSequence)
{
	if (!AnimationSequence)
	{
		return;
	}

	UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, FName(TEXT("Speed")), false);
}

