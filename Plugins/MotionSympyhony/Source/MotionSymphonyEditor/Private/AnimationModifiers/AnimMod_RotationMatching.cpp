//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimationModifiers/AnimMod_RotationMatching.h"
#include "Animation/AnimSequence.h"

void UAnimMod_RotationMatching::OnApply_Implementation(UAnimSequence* AnimationSequence)
{
	if (!AnimationSequence)
	{
		return;
	}

	//Clear or add the releveant curves
	FName CurveName = FName(TEXT("MoSymph_Rotation"));

	bool bCurveExists = UAnimationBlueprintLibrary::DoesCurveExist(AnimationSequence,
		CurveName, ERawCurveTrackTypes::RCT_Float);

	if (bCurveExists)
	{
		//Clear curve
		UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, CurveName, false);
	}

	UAnimationBlueprintLibrary::AddCurve(AnimationSequence, CurveName, ERawCurveTrackTypes::RCT_Float, false);

	//Find the Distance Matching Notify and record the time that it sits at
	float MarkerTime = 0.0f;
	FName DistanceMarkerName = FName(TEXT("DistanceMarker"));
	for (FAnimNotifyEvent& NotifyEvent : AnimationSequence->Notifies)
	{
		if (NotifyEvent.NotifyName == DistanceMarkerName)
		{
			MarkerTime = NotifyEvent.GetTriggerTime();
			break;
		}
	}

	int32 NumSampleFrames = AnimationSequence->GetNumberOfSampledKeys();
	float FrameRate = AnimationSequence->GetSamplingFrameRate().AsDecimal();

	//Calculate FrameRate and Marker Frame
	int32 MarkerFrame = (int32)FMath::RoundHalfToZero(FrameRate * MarkerTime);

	//Add keys for cumulative distance leading up to the marker
	float CumRotation = 0.0f;;
	float FrameDelta = 1.0f / FrameRate;
	UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, CurveName, FrameDelta * MarkerFrame, 0.0f);
	for (int32 i = 1; i < MarkerFrame; ++i)
	{
		float StartTime = FrameDelta * (MarkerFrame - i);
		float YawDelta = AnimationSequence->ExtractRootMotion(StartTime, FMath::Abs(FrameDelta), false).Rotator().Yaw;

		CumRotation += FMath::Abs(YawDelta);

		UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, CurveName, StartTime, CumRotation);

	}

	//Add keys for cumulative distance beyond the marker
	CumRotation = 0.0f;

	for (int32 i = MarkerFrame + 1; i < NumSampleFrames; ++i)
	{
		float StartTime = FrameDelta * i;
		float YawDelta = AnimationSequence->ExtractRootMotion(StartTime - FrameDelta, FMath::Abs(FrameDelta), false).Rotator().Yaw;

		CumRotation -= FMath::Abs(YawDelta);

		UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, CurveName, StartTime, CumRotation);
	}
}

void UAnimMod_RotationMatching::OnRevert_Implementation(UAnimSequence* AnimationSequence)
{
	if (!AnimationSequence)
	{
		return;
	}

	UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, FName(TEXT("MoSymph_Rotation")), false);
}
