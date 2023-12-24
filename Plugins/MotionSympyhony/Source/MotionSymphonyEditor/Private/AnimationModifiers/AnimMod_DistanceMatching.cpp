//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimationModifiers/AnimMod_DistanceMatching.h"
#include "AnimationBlueprintLibrary.h"
#include "Animation/AnimSequence.h"

void UAnimMod_DistanceMatching::OnApply_Implementation(UAnimSequence* AnimationSequence)
{
	if(!AnimationSequence)
	{
		return;
	}

	//Clear or add the releveant curves
	FName CurveName = FName(TEXT("MoSymph_Distance"));

	bool bCurveExists = UAnimationBlueprintLibrary::DoesCurveExist(AnimationSequence, 
		CurveName , ERawCurveTrackTypes::RCT_Float);

	if (bCurveExists)
	{
		//Clear curve
		UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, CurveName, false);
	}

	UAnimationBlueprintLibrary::AddCurve(AnimationSequence, CurveName, ERawCurveTrackTypes::RCT_Float, false);

	//Find the Distance Matching Notify and record the time that it sits at
	float MarkerTime = 0.0f;
	const FName DistanceMarkerName = FName(TEXT("DistanceMarker"));
	for (FAnimNotifyEvent& NotifyEvent : AnimationSequence->Notifies)
	{
		if (NotifyEvent.NotifyName == DistanceMarkerName)
		{
			MarkerTime = NotifyEvent.GetTriggerTime();
			break;
		}
	}

	//Calculate FrameRate and Marker Frame
	const float FrameRate = static_cast<float>(AnimationSequence->GetSamplingFrameRate().AsDecimal());
	const int32 MarkerFrame = static_cast<int32>(FMath::RoundHalfToZero(FrameRate * MarkerTime));

	//Add keys for cumulative distance leading up to the marker
	float CumDistance = 0.0f;

	const float FrameDelta = 1.0f / FrameRate;
	UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, CurveName, FrameDelta * MarkerFrame, 0.0f);
	for(int32 i = 1; i < MarkerFrame; ++i)
	{
		const float StartTime = FrameDelta * (MarkerFrame - i);
		FVector MoveDelta = AnimationSequence->ExtractRootMotion(StartTime, FMath::Abs(FrameDelta), false).GetLocation();
		MoveDelta.Z = 0.0f;

		CumDistance += MoveDelta.Size();

		UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, CurveName, StartTime, CumDistance);

	}

	//Add keys for cumulative distance beyond the marker
	CumDistance = 0.0f;
	const int32 NumSampleFrames = AnimationSequence->GetNumberOfSampledKeys();

	for (int32 i = MarkerFrame + 1; i < NumSampleFrames; ++i)
	{
		const float StartTime = FrameDelta * i;
		FVector MoveDelta = AnimationSequence->ExtractRootMotion(StartTime - FrameDelta, FMath::Abs(FrameDelta), false).GetLocation();
		MoveDelta.Z = 0.0f;

		CumDistance -= MoveDelta.Size();

		UAnimationBlueprintLibrary::AddFloatCurveKey(AnimationSequence, CurveName, StartTime, CumDistance);
	}
}

void UAnimMod_DistanceMatching::OnRevert_Implementation(UAnimSequence* AnimationSequence)
{
	if (!AnimationSequence)
	{
		return;
	}

	UAnimationBlueprintLibrary::RemoveCurve(AnimationSequence, FName(TEXT("MoSymph_Distance")), false);
}
