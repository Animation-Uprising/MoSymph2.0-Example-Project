//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimGraph/AnimNode_TransitionMatching.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "AnimGraph/AnimNode_MotionRecorder.h"
#include "Runtime/Launch/Resources/Version.h"

FTransitionAnimData::FTransitionAnimData()
	: AnimSequence(nullptr),
	CurrentMove(FVector(0.0f)),
	DesiredMove(FVector(0.0f)),
	TransitionDirectionMethod(ETransitionDirectionMethod::Manual),
	CostMultiplier(1.0f),
	bMirror(false),
	StartPose(0),
	EndPose(0)
{
}

FTransitionAnimData::FTransitionAnimData(const FTransitionAnimData& CopyTransition, const bool bInMirror /*= false*/)
	: AnimSequence(CopyTransition.AnimSequence),
	CurrentMove(CopyTransition.CurrentMove),
	DesiredMove(CopyTransition.DesiredMove),
	TransitionDirectionMethod(CopyTransition.TransitionDirectionMethod),
	CostMultiplier(CopyTransition.CostMultiplier),
	bMirror(bInMirror),
	StartPose(CopyTransition.StartPose),
	EndPose(CopyTransition.EndPose)
{
	if (bInMirror)
	{
		StartPose = 0;
		EndPose = 0;
		CurrentMove.X *= -1;
		DesiredMove.X *= -1;
	}
}

FAnimNode_TransitionMatching::FAnimNode_TransitionMatching()
	: CurrentMoveVector(FVector(0.0f)),
	  DesiredMoveVector(FVector(0.0f)),
	  TransitionMatchingOrder(ETransitionMatchingOrder::TransitionPriority),
	  StartDirectionWeight(1.0f),
	  EndDirectionWeight(1.0f),
	  DistanceMatchingUseCase(EDistanceMatchingUseCase::None),
	  DesiredDistance(0.0f),
	  MatchDistanceModule(nullptr)
{
}

void FAnimNode_TransitionMatching::FindMatchPose(const FAnimationUpdateContext& Context)
{
	if (Poses.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("FAnimNode_TransitionMatching: No poses recorded in node"))
			return;
	}

	if (TransitionAnimData.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("FAnimNode_TransitionMatching: No TransitionAnimData, cannot find a pose"))
		return;
	}
	
	FAnimNode_MotionRecorder* MotionRecorderNode = nullptr;
	IMotionSnapper* MotionSnapper = Context.GetMessage<IMotionSnapper>();
	if (MotionSnapper)
	{
		MotionRecorderNode = &MotionSnapper->GetNode();
	}

	if(MotionRecorderNode)
	{
		int32 MinimaCostPoseId = 0;
		if(const TArray<float>* CurrentPoseArray = MotionRecorderNode->GetCurrentPoseArray(PoseRecorderConfigIndex))
		{
			switch(DistanceMatchingUseCase)
			{
			case EDistanceMatchingUseCase::None:
				{
					switch (TransitionMatchingOrder)
					{
					case ETransitionMatchingOrder::TransitionPriority: MinimaCostPoseId = GetMinimaCostPoseId_TransitionPriority(*CurrentPoseArray); break;
					case ETransitionMatchingOrder::PoseAndTransitionCombined: MinimaCostPoseId = GetMinimaCostPoseId_PoseTransitionWeighted(*CurrentPoseArray); break;
					}
				} break;
			case EDistanceMatchingUseCase::Strict:
				{
					MinimaCostPoseId = GetMinimaCostPoseId_TransitionPriority_Distance();
				} break;
			}

			MinimaCostPoseId = FMath::Clamp(MinimaCostPoseId, 0, Poses.Num() - 1);
		}

		MatchPose = &Poses[MinimaCostPoseId];
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FAnimNode_PoseMatchBase: Cannot find Motion Snapshot node to pose match against."))

		int32 MinimaTransitionId = 0;
		float MinimaTransitionCost = 10000000.0f;
		for (int32 i = 0; i < TransitionAnimData.Num(); ++i)
		{
			FTransitionAnimData& TransitionData = TransitionAnimData[i];

			const float CurrentVectorDelta = FVector::DistSquared(CurrentMoveVector, TransitionData.CurrentMove);
			const float DesiredVectorDelta = FVector::DistSquared(DesiredMoveVector, TransitionData.DesiredMove);

			const float Cost = (CurrentVectorDelta * StartDirectionWeight) + (DesiredVectorDelta * EndDirectionWeight)
				* TransitionData.CostMultiplier;

			if (Cost < MinimaTransitionCost)
			{
				MinimaTransitionCost = Cost;
				MinimaTransitionId = i;
			}
		}

		MinimaTransitionId = FMath::Clamp(MinimaTransitionId, 0, TransitionAnimData.Num() - 1);
		const int32 MatchPoseId = FMath::Clamp(TransitionAnimData[MinimaTransitionId].StartPose, 0, Poses.Num() - 1);
		MatchPose = &Poses[MatchPoseId];
	}
	
	SetSequence(FindActiveAnim());
	
	InternalTimeAccumulator = MatchPose->Time;
	SetStartPosition(InternalTimeAccumulator);
	PlayRateScaleBiasClampState.Reinitialize();
}

UAnimSequenceBase* FAnimNode_TransitionMatching::FindActiveAnim()
{
	if (TransitionAnimData.Num() == 0)
	{
		return nullptr;
	}

	const int32 AnimId = FMath::Clamp(MatchPose->AnimId, 0, TransitionAnimData.Num() - 1);

	return TransitionAnimData[AnimId].AnimSequence;
}

int32 FAnimNode_TransitionMatching::GetMinimaCostPoseId_TransitionPriority(const TArray<float>& InCurrentPoseArray)
{
	//First find out which transition set is the best match
	FTransitionAnimData* MinimaCostSet = nullptr;
	float MinimaCost = 10000000.0f;
	for(FTransitionAnimData& TransitionData : TransitionAnimData)
	{
		const float CurrentVectorDelta = FVector::DistSquared(CurrentMoveVector, TransitionData.CurrentMove);
		const float DesiredVectorDelta = FVector::DistSquared(DesiredMoveVector, TransitionData.DesiredMove);

		float SetCost = (CurrentVectorDelta * StartDirectionWeight) + (DesiredVectorDelta * EndDirectionWeight);

		SetCost *= TransitionData.CostMultiplier;

		if (SetCost < MinimaCost)
		{
			MinimaCost = SetCost;
			MinimaCostSet = &TransitionData;
		}
	}

	//Search mirrored transitions as well if mirroring is enabled
	if(bEnableMirroring)
	{
		for (FTransitionAnimData& TransitionData : MirroredTransitionAnimData)
		{
			const float CurrentVectorDelta = FVector::DistSquared(CurrentMoveVector, TransitionData.CurrentMove);
			const float DesiredVectorDelta = FVector::DistSquared(DesiredMoveVector, TransitionData.DesiredMove);

			float SetCost = (CurrentVectorDelta * StartDirectionWeight) + (DesiredVectorDelta * EndDirectionWeight);

			SetCost *= TransitionData.CostMultiplier;

			if (SetCost < MinimaCost)
			{
				MinimaCost = SetCost;
				MinimaCostSet = &TransitionData;
			}
		}
	}

	//Within the chosen transition (MinimaCostSet) Find the best pose to match to
	float Cost = 0.0f;
	return GetMinimaCostPoseId(InCurrentPoseArray, Cost, MinimaCostSet->StartPose, MinimaCostSet->EndPose);
}

int32 FAnimNode_TransitionMatching::GetMinimaCostPoseId_PoseTransitionWeighted(const TArray<float>& InCurrentPoseArray)
{
	int32 MinimaCostPoseId = 0;
	float MinimaCost = 10000000.0f;
	for (FTransitionAnimData& TransitionData : TransitionAnimData)
	{
		const float CurrentVectorDelta = FVector::DistSquared(CurrentMoveVector, TransitionData.CurrentMove);
		const float DesiredVectorDelta = FVector::DistSquared(DesiredMoveVector, TransitionData.DesiredMove);

		//Find the Lowest Cost Pose from this transition anim data 
		int32 SetMinimaPoseId = -1;
		float SetMinimaCost = 10000000.0f;

		SetMinimaPoseId = GetMinimaCostPoseId(InCurrentPoseArray, SetMinimaCost, TransitionData.StartPose, TransitionData.EndPose);

		//Add Transition direction cost
		SetMinimaCost += (CurrentVectorDelta * StartDirectionWeight) + (DesiredVectorDelta * EndDirectionWeight);

		//Apply CostMultiplier
		SetMinimaCost *= TransitionData.CostMultiplier;

		if (SetMinimaCost < MinimaCost)
		{
			MinimaCost = SetMinimaCost;
			MinimaCostPoseId = SetMinimaPoseId;
		}
	}

	if(bEnableMirroring)
	{
		for (FTransitionAnimData& TransitionData : MirroredTransitionAnimData)
		{
			const float CurrentVectorDelta = FVector::DistSquared(CurrentMoveVector, TransitionData.CurrentMove);
			const float DesiredVectorDelta = FVector::DistSquared(DesiredMoveVector, TransitionData.DesiredMove);

			//Find the Lowest Cost Pose from this transition anim data 
			float SetMinimaCost = 10000000.0f;
			const int32 SetMinimaPoseId = GetMinimaCostPoseId(InCurrentPoseArray, SetMinimaCost,
																TransitionData.StartPose, TransitionData.EndPose);

			//Add Transition direction cost
			SetMinimaCost += (CurrentVectorDelta * StartDirectionWeight) + (DesiredVectorDelta * EndDirectionWeight);

			//Apply CostMultiplier
			SetMinimaCost *= TransitionData.CostMultiplier;

			if (SetMinimaCost < MinimaCost)
			{
				MinimaCost = SetMinimaCost;
				MinimaCostPoseId = SetMinimaPoseId;
			}
		}
	}

	return MinimaCostPoseId;
}

int32 FAnimNode_TransitionMatching::GetMinimaCostPoseId_TransitionPriority_Distance()
{
	//First find out which transition set is the best match
	FTransitionAnimData* MinimaCostSet = nullptr;
	float MinimaCost = 10000000.0f;
	for(FTransitionAnimData& TransitionData : TransitionAnimData)
	{
		const float CurrentVectorDelta = FVector::DistSquared(CurrentMoveVector, TransitionData.CurrentMove);
		const float DesiredVectorDelta = FVector::DistSquared(DesiredMoveVector, TransitionData.DesiredMove);

		float SetCost = (CurrentVectorDelta * StartDirectionWeight) + (DesiredVectorDelta * EndDirectionWeight);

		SetCost *= TransitionData.CostMultiplier;

		if (SetCost < MinimaCost)
		{
			MinimaCost = SetCost;
			MinimaCostSet = &TransitionData;
		}
	}

	//Search mirrored transitions as well if mirroring is enabled
	if(bEnableMirroring)
	{
		for (FTransitionAnimData& TransitionData : MirroredTransitionAnimData)
		{
			const float CurrentVectorDelta = FVector::DistSquared(CurrentMoveVector, TransitionData.CurrentMove);
			const float DesiredVectorDelta = FVector::DistSquared(DesiredMoveVector, TransitionData.DesiredMove);

			float SetCost = (CurrentVectorDelta * StartDirectionWeight) + (DesiredVectorDelta * EndDirectionWeight);

			SetCost *= TransitionData.CostMultiplier;

			if (SetCost < MinimaCost)
			{
				MinimaCost = SetCost;
				MinimaCostSet = &TransitionData;
			}
		}
	}

	//Within the chosen transition (MinimaCostSet) Find the best pose to match to
	if(!MinimaCostSet)
	{
		return 0;
	}
	
	const float DesiredTime = MinimaCostSet->DistanceMatchModule.FindMatchingTime(
		DesiredDistance, DistanceMatchData.bNegateDistanceCurve);

	const int32 DesiredPose = MinimaCostSet->StartPose + FMath::RoundHalfToZero(DesiredTime / PoseInterval);

	MatchDistanceModule = &MinimaCostSet->DistanceMatchModule;
	
	return FMath::Clamp(DesiredPose, MinimaCostSet->StartPose, MinimaCostSet->EndPose);
}

int32 FAnimNode_TransitionMatching::GetMinimaCostPoseId_PoseTransitionWeighted_Distance()
{
	float MinimaCost = 100000.0f;
	FTransitionAnimData*  MinimaCostTransition = nullptr;

	for (FTransitionAnimData& TransitionData : TransitionAnimData)
	{
		const float CurrentVectorDelta = FVector::DistSquared(CurrentMoveVector, TransitionData.CurrentMove);
		const float DesiredVectorDelta = FVector::DistSquared(DesiredMoveVector, TransitionData.DesiredMove);

		//Add Transition direction cost
		float Cost = (CurrentVectorDelta * StartDirectionWeight) + (DesiredVectorDelta * EndDirectionWeight);

		//Apply CostMultiplier
		Cost *= TransitionData.CostMultiplier;

		if (Cost < MinimaCost)
		{
			MinimaCost = Cost;
			MinimaCostTransition = &TransitionData;
		}
	}

	if(bEnableMirroring)
	{
		for (FTransitionAnimData& TransitionData : MirroredTransitionAnimData)
		{
			const float CurrentVectorDelta = FVector::DistSquared(CurrentMoveVector, TransitionData.CurrentMove);
			const float DesiredVectorDelta = FVector::DistSquared(DesiredMoveVector, TransitionData.DesiredMove);

			//Add Transition direction cost
			float Cost = (CurrentVectorDelta * StartDirectionWeight) + (DesiredVectorDelta * EndDirectionWeight);

			//Apply CostMultiplier
			Cost *= TransitionData.CostMultiplier;

			if (Cost < MinimaCost)
			{
				MinimaCost = Cost;
				MinimaCostTransition = &TransitionData;
			}
		}
	}

	if(!MinimaCostTransition)
	{
		return 0;
	}

	const float AnimTime = MinimaCostTransition->DistanceMatchModule.FindMatchingTime(DesiredDistance, DistanceMatchData.bNegateDistanceCurve);
	const int32 PoseId = FMath::RoundHalfToZero(AnimTime / PoseInterval);
	
	return FMath::Clamp(PoseId, MinimaCostTransition->StartPose, MinimaCostTransition->EndPose);
}

int32 FAnimNode_TransitionMatching::GetAnimationIndex(UAnimSequence* AnimSequence)
{
	if(!AnimSequence)
	{
		return 0;
	}

	for (int32 i = 0; i < TransitionAnimData.Num(); ++i)
	{
		if (AnimSequence == TransitionAnimData[i].AnimSequence)
		{
			return i;
		}
	}

	return 0;
}

USkeleton* FAnimNode_TransitionMatching::GetNodeSkeleton()
{
	if(TransitionAnimData.Num() == 0)
	{
		return nullptr;
	}

	for(FTransitionAnimData& TransitionData : TransitionAnimData)
	{
		if(TransitionData.AnimSequence)
		{
			return TransitionData.AnimSequence->GetSkeleton();
		}
	}

	return nullptr;
}

#if WITH_EDITOR
void FAnimNode_TransitionMatching::PreProcess()
{
	FAnimNode_PoseMatchBase::PreProcess();

	//Find First valid animation
	FTransitionAnimData* FirstValidTransitionData = nullptr;
	for (FTransitionAnimData& TransitionData : TransitionAnimData)
	{
		if (TransitionData.AnimSequence != nullptr)
		{
			FirstValidTransitionData = &TransitionData;
			break;
		}
	}

	if (FirstValidTransitionData)
	{
		return;
	}

	if(!FirstValidTransitionData->AnimSequence->IsValidToPlay())
	{
		return;
	}

	for (int32 i = 0; i < TransitionAnimData.Num(); ++i)
	{
		FTransitionAnimData& TransitionData = TransitionAnimData[i];

		if (!TransitionData.AnimSequence
			|| !TransitionData.AnimSequence->IsValidToPlay())
		{
			continue;
		}

		TransitionData.StartPose = Poses.Num();

		PreProcessAnimation(TransitionData.AnimSequence, i);

		if (TransitionData.TransitionDirectionMethod == ETransitionDirectionMethod::RootMotion)
		{
			if (TransitionData.AnimSequence->HasRootMotion())
			{
				const float SequenceLength = TransitionData.AnimSequence->GetPlayLength();

				FTransform StartRootMotion = TransitionData.AnimSequence->ExtractRootMotion(0.0f, 0.05f, false);
				TransitionData.CurrentMove = StartRootMotion.GetLocation().GetSafeNormal();

				FTransform RootMotion = TransitionData.AnimSequence->ExtractRootMotion(0.0f, SequenceLength, false);
				TransitionData.DesiredMove = RootMotion.GetLocation().GetSafeNormal();
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AnimNode_TransitionMatching: Attempting to extract transition direction from root motion, but this sequence does not contain root motion"));
			}
		}

		TransitionData.EndPose = Poses.Num() - 1;

		//Copy the transition data for mirroring if mirroring is enabled for this transition
		if (bEnableMirroring && TransitionData.bMirror)
		{
			MirroredTransitionAnimData.Emplace(FTransitionAnimData(TransitionData, true));
		}
	}

	//Pre-Process Mirroring
	if (bEnableMirroring)
	{
		for (int32 i = 0; i < MirroredTransitionAnimData.Num(); ++i)
		{
			FTransitionAnimData& TransitionData = MirroredTransitionAnimData[i];

			if (!TransitionData.AnimSequence)
			{
				continue;
			}

			TransitionData.StartPose = Poses.Num();

			PreProcessAnimation(TransitionData.AnimSequence, GetAnimationIndex(TransitionData.AnimSequence), true);

			TransitionData.EndPose = Poses.Num() - 1;
		}
	}
}
#endif

void FAnimNode_TransitionMatching::InitializeData()
{
	FAnimNode_PoseMatchBase::InitializeData();

	if(TransitionAnimData.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to initialize multi-pose matching node. There are no animation sequences set."));
		return;
	}
	
	if(DistanceMatchingUseCase == EDistanceMatchingUseCase::Strict)
	{
		for(int32 i = 0; i < TransitionAnimData.Num(); ++i)
			for(FTransitionAnimData& TransitionData : TransitionAnimData)
			{
				if(TransitionData.AnimSequence)
				{
					TransitionData.DistanceMatchModule.Setup(TransitionData.AnimSequence, DistanceMatchData.DistanceCurveName);
				}
			}
	}
}

void FAnimNode_TransitionMatching::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);

	if(DistanceMatchingUseCase == EDistanceMatchingUseCase::Strict)
	{
		MatchDistanceModule = nullptr;
		MatchPose = nullptr;
		
		for(FTransitionAnimData& TransitionData : TransitionAnimData)
		{
			TransitionData.DistanceMatchModule.Initialize();
		}
	}
}

void FAnimNode_TransitionMatching::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	if(DistanceMatchingUseCase == EDistanceMatchingUseCase::None)
	{
		Super::UpdateAssetPlayer(Context);
		return;
	}
	
	GetEvaluateGraphExposedInputs().Execute(Context);

	if(bInitPoseSearch)
	{
		FindMatchPose(Context);

		UAnimSequenceBase* CacheSequence = GetSequence();

		if(MatchPose && CacheSequence && MatchDistanceModule)
		{
			const float CachePlayRateBasis = GetPlayRateBasis();
			
			InternalTimeAccumulator = FMath::Clamp(GetStartPosition(), 0.0f, CacheSequence->GetPlayLength());
			SetStartPosition(InternalTimeAccumulator);
			const float AdjustedPlayRate = PlayRateScaleBiasClampState.ApplyTo(GetPlayRateScaleBiasClampConstants(),
				FMath::IsNearlyZero(CachePlayRateBasis) ? 0.0f : (GetPlayRate() / CachePlayRateBasis), Context.GetDeltaTime());
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
	else if(MatchDistanceModule)
	{
		const float DistanceBasedTime = DistanceMatchData.GetDistanceMatchingTime(MatchDistanceModule,
			DesiredDistance, InternalTimeAccumulator);

		if(DistanceBasedTime > -FLT_EPSILON)
		{
			InternalTimeAccumulator = DistanceBasedTime;
		}
		else
		{
			FAnimNode_SequencePlayer::UpdateAssetPlayer(Context);
		}
	}
}

