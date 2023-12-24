//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimGraph/AnimNode_MSDistanceMatching.h"
#include "Animation/AnimInstanceProxy.h"

#define LOCTEXT_NAMESPACE "MotionSymphonyNodes"

static TAutoConsoleVariable<int32> CVarDistanceMatchingEnabled(
	TEXT("a.AnimNode.MoSymph.DistanceMatch.Enabled"),
	1,
	TEXT("Turns Distance Matching On / Off. \n")
	TEXT("<=0: Off \n")
	TEXT("  1: On"));


FAnimNode_MSDistanceMatching::FAnimNode_MSDistanceMatching()
	: DesiredDistance(0.0f),
	DistanceCurveName(FName(TEXT("MoSymph_Distance"))),
	bNegateDistanceCurve(false),
	MovementType(EDistanceMatchType::None),
	DistanceLimit(-1.0f),
	DestinationReachedThreshold(5.0f),
	SmoothRate(-1.0f),
	SmoothTimeThreshold(0.15f),
	LastAnimSequenceUsed(nullptr)
{
}

void FAnimNode_MSDistanceMatching::InitializeData()
{
	UAnimSequenceBase* CacheSequence = GetSequence();
	
	if (!CacheSequence)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to initialize distance matching node. The sequence is null and has not been set"));
		return;
	}

	DistanceMatchingModule.Setup(CacheSequence, DistanceCurveName);
	LastAnimSequenceUsed = CacheSequence;

	const float CachePlayRateBasis = GetPlayRateBasis();
	
	const float AdjustedPlayRate = PlayRateScaleBiasClampState.ApplyTo(GetPlayRateScaleBiasClampConstants(),
		FMath::IsNearlyZero(CachePlayRateBasis) ? 0.0f : (GetPlayRate() / CachePlayRateBasis), 0.0f);
	const float EffectivePlayRate = CacheSequence->RateScale * AdjustedPlayRate;

	if (GetStartPosition() == 0.0f && EffectivePlayRate < 0.0f)
	{
		InternalTimeAccumulator = CacheSequence->GetPlayLength();
	}

	bInitialized = true;
}

void FAnimNode_MSDistanceMatching::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_AssetPlayerBase::Initialize_AnyThread(Context);
	GetEvaluateGraphExposedInputs().Execute(Context);

	if(!bInitialized)
	{
		InitializeData();
	}

	InternalTimeAccumulator = 0.0f;
	SetStartPosition(0.0f);

	//Check if the user has changed the animation. If so we need to re-setup the distance matching module
	//This is not the recommended workflow. Multi-Pose matching nodes (with distance matching enabled) should be used instead for performance
	UAnimSequenceBase* CacheSequence = GetSequence();
	if (CacheSequence != LastAnimSequenceUsed)
	{
		DistanceMatchingModule.Setup(CacheSequence, DistanceCurveName);
		LastAnimSequenceUsed = CacheSequence;
	}

	DistanceMatchingModule.Initialize();
}

void FAnimNode_MSDistanceMatching::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	GetEvaluateGraphExposedInputs().Execute(Context);

	UAnimSequenceBase* CacheSequence = GetSequence();
	if(!CacheSequence)
	{
		return;
	}

	const int32 Enabled = CVarDistanceMatchingEnabled.GetValueOnAnyThread();
	
	if (Enabled 
		&& (DistanceLimit < 0.0f || DesiredDistance < DistanceLimit))
	{
		//Evaluate Distance matching time
		float Time = -1.0f;
		const bool bDestinationReached = (MovementType == EDistanceMatchType::Forward 
			&& DesiredDistance < DestinationReachedThreshold);

		if (!bDestinationReached)
		{
			Time = DistanceMatchingModule.FindMatchingTime(DesiredDistance, bNegateDistanceCurve);
		}

		if (Time > -0.5f)
		{
			//Clamp the time so that it cannot be beyond clip limits and so that the animation cannot run backward
			if (SmoothRate > 0.0f && FMath::Abs(Time - InternalTimeAccumulator) < SmoothTimeThreshold)
			{
				const float DesiredTime = FMath::Clamp(Time, 0.0f /*InternalTimeAccumulator*/, CacheSequence->GetPlayLength());
				InternalTimeAccumulator = FMath::Lerp(InternalTimeAccumulator, DesiredTime, SmoothRate);
			}
			else
			{
				InternalTimeAccumulator = FMath::Clamp(Time, 0.0f /*InternalTimeAccumulator*/, CacheSequence->GetPlayLength());
			}

		}
		else
		{
			FAnimNode_SequencePlayer::UpdateAssetPlayer(Context);
		}
	}
	else
	{
		FAnimNode_SequencePlayer::UpdateAssetPlayer(Context);
	}
}

#undef LOCTEXT_NAMESPACE