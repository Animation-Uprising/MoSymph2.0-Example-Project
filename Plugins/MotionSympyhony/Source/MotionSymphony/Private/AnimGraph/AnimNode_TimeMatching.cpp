//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimGraph/AnimNode_TimeMatching.h"
#include "Animation/AnimInstanceProxy.h"

#define LOCTEXT_NAMESPACE "MotionSymphonyNodes"

static TAutoConsoleVariable<int32> CVarTimeMatchingEnabled(
	TEXT("a.AnimNode.MoSymph.TimeMatch.Enabled"),
	1,
	TEXT("Turns Time Matching On / Off. \n")
	TEXT("<=0: Off \n")
	TEXT("  1: On"));


FAnimNode_TimeMatching::FAnimNode_TimeMatching()
	: DesiredTime(0.0f),
	  MarkerTime(0.0f),
	  bInitialized(false),
	  DistanceMatching(nullptr),
	  AnimInstanceProxy(nullptr)
{
}

float FAnimNode_TimeMatching::FindMatchingTime() const
{
	return MarkerTime - DesiredTime;
}

void FAnimNode_TimeMatching::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_AssetPlayerBase::Initialize_AnyThread(Context);
	GetEvaluateGraphExposedInputs().Execute(Context);

	InternalTimeAccumulator = GetStartPosition(); //StartPosition;

	UAnimSequenceBase* LocalSequence = GetSequence();
	
	if (!LocalSequence)
	{
		return;
	}

	if (!bInitialized)
	{
		AnimInstanceProxy = Context.AnimInstanceProxy;
		DistanceMatching = Cast<UDistanceMatching>(AnimInstanceProxy->GetSkelMeshComponent()->GetOwner()->GetComponentByClass(UDistanceMatching::StaticClass()));

		bInitialized = true;
	}

	if (DistanceMatching)
	{
		DesiredTime = DistanceMatching->GetTimeToMarker();
	}

	const float LocalPlayRateBasis = GetPlayRateBasis();
	
	const float AdjustedPlayRate = PlayRateScaleBiasClampState.ApplyTo(GetPlayRateScaleBiasClampConstants(),
		FMath::IsNearlyZero(LocalPlayRateBasis) ? 0.0f : (GetPlayRate() / LocalPlayRateBasis), 0.0f);
	const float EffectivePlayRate = LocalSequence->RateScale * AdjustedPlayRate;

	DesiredTime *= EffectivePlayRate;
	if (CVarTimeMatchingEnabled.GetValueOnAnyThread() == 1)
	{
		InternalTimeAccumulator = FindMatchingTime();
	}

	if (GetStartPosition() == 0.0f && EffectivePlayRate < 0.0f)
	{
		InternalTimeAccumulator = LocalSequence->GetPlayLength();
	}
}

#undef LOCTEXT_NAMESPACE