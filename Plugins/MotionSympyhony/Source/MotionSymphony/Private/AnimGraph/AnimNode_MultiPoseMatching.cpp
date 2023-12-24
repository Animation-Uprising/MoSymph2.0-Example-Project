//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimGraph/AnimNode_MultiPoseMatching.h"
#include "Runtime/Launch/Resources/Version.h"

FAnimNode_MultiPoseMatching::FAnimNode_MultiPoseMatching()
	: DistanceMatchingUseCase(EDistanceMatchingUseCase::None),
	  DesiredDistance(0.0f),
	  MatchDistanceModule(nullptr)
{

}

UAnimSequenceBase* FAnimNode_MultiPoseMatching::FindActiveAnim()
{
	if(Animations.Num() == 0)
	{
		return nullptr;
	}

	const int32 AnimId = FMath::Clamp(MatchPose->AnimId, 0, Animations.Num() - 1);

	return Animations[AnimId];
}

#if WITH_EDITOR
void FAnimNode_MultiPoseMatching::PreProcess()
{
	FAnimNode_PoseMatchBase::PreProcess();

	//Find first valid animation
	UAnimSequence* FirstValidSequence = nullptr;
	for (UAnimSequence* CurSequence : Animations)
	{
		if (CurSequence != nullptr)
		{
			FirstValidSequence = CurSequence;
			break;
		}
	}

	if(!FirstValidSequence
		|| !FirstValidSequence->IsValidToPlay())
	{
		return;
	}

	for(int32 i = 0; i < Animations.Num(); ++i)
	{
		if(UAnimSequence* CurSequence = Animations[i])
		{
			if (bEnableMirroring && MirrorDataTable)
			{
				PreProcessAnimation(CurSequence, i, true);
			}
			else
			{
				PreProcessAnimation(CurSequence, i);
			}
		}
	}
}
#endif

void FAnimNode_MultiPoseMatching::InitializeData()
{
	FAnimNode_PoseMatchBase::InitializeData();

	if(Animations.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to initialize multi-pose matching node. There are no animation sequences set."));
		return;
	}
	
	if(DistanceMatchingUseCase == EDistanceMatchingUseCase::Strict)
	{
		DistanceMatchingModules.Empty(Animations.Num()+1);
		DistanceMatchingModules.SetNum(Animations.Num());
		for(int32 i = 0; i < Animations.Num(); ++i)
		{
			if(UAnimSequence* AnimSequence = Animations[i])
			{
				DistanceMatchingModules[i].Setup(AnimSequence, DistanceMatchData.DistanceCurveName);
			}
		}
	}
}

void FAnimNode_MultiPoseMatching::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);

	//Reset all the distance matching modules
	if(DistanceMatchingUseCase == EDistanceMatchingUseCase::Strict)
	{
		for(FDistanceMatchingModule& DistModule : DistanceMatchingModules)
		{
			DistModule.Initialize();
		}
	}
}

void FAnimNode_MultiPoseMatching::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	if(DistanceMatchingUseCase == EDistanceMatchingUseCase::None)
	{
		Super::UpdateAssetPlayer(Context);
		return;
	}

	GetEvaluateGraphExposedInputs().Execute(Context);

	if(bInitPoseSearch) //This is the initial update which finds which animation to distance match on
	{
		FindMatchPose(Context);

		UAnimSequenceBase* CacheSequence = GetSequence();

		if(MatchPose && CacheSequence && MatchDistanceModule)
		{
			const float CachePlayRateBasis = GetPlayRateBasis();
			
			InternalTimeAccumulator = FMath::Clamp(GetStartPosition(), 0.0f, CacheSequence->GetPlayLength());
			SetStartPosition(InternalTimeAccumulator);
			const float AdjustedPlayRate = PlayRateScaleBiasClampState.ApplyTo(GetPlayRateScaleBiasClampConstants(),
				FMath::IsNearlyZero(CachePlayRateBasis) ? 0.0f : (GetPlayRate() / CachePlayRateBasis), 0.0f);
			const float EffectivePlayRate = CacheSequence->RateScale * AdjustedPlayRate;

			if ((MatchPose->Time == 0.0f) && (EffectivePlayRate < 0.0f))
			{
				InternalTimeAccumulator = CacheSequence->GetPlayLength();
			}
#if ENGINE_MINOR_VERSION > 2
			CreateTickRecordForNode(Context, CacheSequence, IsLooping(), AdjustedPlayRate, false);
#else
			CreateTickRecordForNode(Context, CacheSequence, GetLoopAnimation(), AdjustedPlayRate, false);
#endif
		}

		bInitPoseSearch = false;
	}
	else //This is the regular update the time in the chosen animation to distance match on
	{
	
		const float DistanceBasedTime = DistanceMatchData.GetDistanceMatchingTime(MatchDistanceModule,
			DesiredDistance, InternalTimeAccumulator);
		
		if(DistanceBasedTime > -FLT_EPSILON)
		{
			InternalTimeAccumulator = DistanceBasedTime;
		}
		else //Revert to playing the animation as if it were a sequence player
		{
			FAnimNode_SequencePlayer::UpdateAssetPlayer(Context);
		}
	}
	
}

USkeleton* FAnimNode_MultiPoseMatching::GetNodeSkeleton()
{
	if(Animations.Num() == 0)
	{
		return nullptr;
	}

	for(UAnimSequence* AnimSequence : Animations)
	{
		if(AnimSequence)
		{
			return AnimSequence->GetSkeleton();
		}
	}

	return nullptr;
}

int32 FAnimNode_MultiPoseMatching::GetMinimaCostPoseId(const TArray<float>* InCurrentPoseArray)
{
	if(!InCurrentPoseArray)
	{
		return 0;
	}
	
	if(DistanceMatchingUseCase == EDistanceMatchingUseCase::None)
	{
		return Super::GetMinimaCostPoseId(InCurrentPoseArray);
	}

	int32 LastPoseChecked = -1;
	int32 LowestCostPoseId = -1;
	float LowestPoseCost = 1000000.0f;
	for(int32 i = 0; i < DistanceMatchingModules.Num(); ++i)
	{
		FDistanceMatchingModule& DistModule = DistanceMatchingModules[i];
		
		//Find the matching time on this candidate animations distance module
		const float Time = DistModule.FindMatchingTime(DesiredDistance, DistanceMatchData.bNegateDistanceCurve);

		//Find out which pose this time represents in this animation
		float ClosestPoseTimeDif = 100000.0f;
		int32 ClosestPoseId = -1;
		for(int32 j = LastPoseChecked + 1; j < Poses.Num(); ++j)
		{
			const FPoseMatchData& Pose = Poses[j];

			if(Pose.AnimId > i)
			{
				break;
			}

			const float TimeDistance = FMath::Abs(Pose.Time - Time);

			if(TimeDistance < ClosestPoseTimeDif)
			{
				ClosestPoseTimeDif = TimeDistance;
				ClosestPoseId = j;
			}

			LastPoseChecked = j;
		}

		//Now calculate this pose's cost and check if it is the lowest cost overall
		const float PoseCost = ComputeSinglePoseCost(*InCurrentPoseArray, ClosestPoseId);

		if(PoseCost < LowestPoseCost)
		{
			LowestCostPoseId = ClosestPoseId;
			LowestPoseCost = PoseCost;
		}
	}

	LowestCostPoseId = FMath::Clamp(LowestCostPoseId, 0, Poses.Num() - 1);
	
	//Set the current animation and distance matching module based on the lowest cost pose
	const int32 AnimId = Poses[LowestCostPoseId].AnimId;
	SetSequence(Animations[AnimId]);
	MatchDistanceModule = &DistanceMatchingModules[AnimId];

	return LowestCostPoseId;
}
