//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimGraph/AnimNode_MSMotionMatching.h"
#include "AITypes.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "DrawDebugHelpers.h"
#include "MotionAnimObject.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_Inertialization.h"
#include "Enumerations/EMotionMatchingEnums.h"
#include "Utility/MotionMatchingUtils.h"
#include "Animation/AnimSyncScope.h"
#include "Animation/MirrorDataTable.h"

static TAutoConsoleVariable<int32> CVarMMSearchDebug(
	TEXT("a.AnimNode.MoSymph.MMSearch.Debug"),
	0,
	TEXT("Turns Motion Matching Search Debugging On / Off.\n")
	TEXT("<=0: Off \n")
	TEXT("  1: On - Candidate Trajectory Debug\n")
	TEXT("  2: On - Optimisation Error Debugging\n"));

static TAutoConsoleVariable<int32> CVarMMTrajectoryDebug(
	TEXT("a.AnimNode.MoSymph.MMTrajectory.Debug"),
	0,
	TEXT("Turns Motion Matching Trajectory Debugging On / Off. \n")
	TEXT("<=0: Off \n")
	TEXT("  1: On - Show Desired Trajectory\n")
	TEXT("  2: On - Show Chosen Trajectory\n"));

static TAutoConsoleVariable<int32> CVarMMPoseDebug(
	TEXT("a.AnimNode.MoSymph.MMPose.Debug"),
	0,
	TEXT("Turns Motion Matching Pose Debugging On / Off. \n")
	TEXT("<=0: Off \n")
	TEXT("  1: On - Show Pose Position\n")
	TEXT("  2: On - Show Pose Position and Velocity"));

static TAutoConsoleVariable<int32> CVarMMAnimDebug(
	TEXT("a.AnimNode.MoSymph.MMAnim.Debug"),
	0,
	TEXT("Turns on animation debugging for Motion Matching On / Off. \n")
	TEXT("<=0: Off \n")
	TEXT("  2: On - Show Current Anim Info"));

void FMotionMatchingInputData::Empty(const int32 Size)
{
	DesiredInputArray.Empty(Size);
}

FAnimNode_MSMotionMatching::FAnimNode_MSMotionMatching() :
	bUserForcePoseSearch(false),
	UpdateInterval(0.1f),
	PlaybackRate(1.0f),
	BlendTime(0.3f),
	OverrideQualityVsResponsivenessRatio(0.5f),
	TransitionMethod(ETransitionMethod::Inertialization),
	PastTrajectoryMode(EPastTrajectoryMode::ActualHistory),
	bBlendInputResponse(false),
	InputResponseBlendMagnitude(1.0f),
	bFavourCurrentPose(false),
	CurrentPoseFavour(0.95f),
	NextNaturalRange(0.2f),
	bNextNaturalToleranceTest(false),
	bFavourNextNatural(false),
	NextNaturalFavour(0.95f),
	bEnableToleranceTest(true),
	PositionTolerance(50.0f),
	RotationTolerance(2.0f),
	CurrentActionId(0),
	CurrentActionTime(0),
	CurrentActionEndTime(0),
	TimeSinceMotionUpdate(0.0f),
	TimeSinceMotionChosen(0.0f),
	PoseInterpolationValue(0.0f),
	bForcePoseSearch(false),
	CurrentChosenPoseId(0),
	InputArraySize(0),
	MotionRecorderConfigIndex(-1),
	bValidToEvaluate(false),
	bInitialized(false),
	bTriggerTransition(false),
	AnimInstanceProxy(nullptr)
#if WITH_EDITORONLY_DATA
	, PosesChecked(0),
	InnerAABBsChecked(0),
	InnerAABBsPassed(0),
	OuterAABBsChecked(0),
	OuterAABBsPassed(0),
	AveragePosesCheckedCounter(0),
	AveragePosesChecked(0),
	PosesCheckedMax(INT32_MIN),
	PosesCheckedMin(INT32_MAX)
#endif
{
	InputData.Empty(21);
}

FAnimNode_MSMotionMatching::~FAnimNode_MSMotionMatching()
{

}

void FAnimNode_MSMotionMatching::InitializeWithPoseRecorder(const FAnimationUpdateContext& Context)
{
	FAnimNode_MotionRecorder* MotionRecorderNode = nullptr;
	if (IMotionSnapper* MotionSnapper = Context.GetMessage<IMotionSnapper>())
	{
		MotionRecorderNode = &MotionSnapper->GetNode();
	}

	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	UMotionMatchConfig* MMConfig = CurrentMotionData->MotionMatchConfig;
	
	if(MotionRecorderNode)
	{
		MotionRecorderConfigIndex = MotionRecorderNode->RegisterMotionMatchConfig(MMConfig);
	}
}

void FAnimNode_MSMotionMatching::InitializeMatchedTransition(const FAnimationUpdateContext& Context)
{
	TimeSinceMotionUpdate = TimeSinceMotionChosen = 0.0f;
	
	FAnimNode_MotionRecorder* MotionRecorderNode = nullptr;
	if (IMotionSnapper* MotionSnapper = Context.GetMessage<IMotionSnapper>())
	{
		MotionRecorderNode = &MotionSnapper->GetNode();
	}
	
	if (MotionRecorderNode)
	{
		ComputeCurrentPose(MotionRecorderNode->GetCurrentPoseArray(MotionRecorderConfigIndex));
		TransitionPoseSearch(Context);
	}
	else
	{
		//We just jump to the default pose because there is no way to match to external nodes.
		TransitionToPose(0, Context, 0.0f);
	}
}


void FAnimNode_MSMotionMatching::UpdateMotionMatchingState(const float DeltaTime, const FAnimationUpdateContext& Context)
{
	if (bTriggerTransition)
	{
		InitializeMatchedTransition(Context);
		bTriggerTransition = false;
	}
	else
	{
		UpdateMotionMatching(DeltaTime, Context);
	}
	
	MMAnimState.Update(DeltaTime, PlaybackRate);
	InternalTimeAccumulator = MMAnimState.AnimTime;
}

void FAnimNode_MSMotionMatching::UpdateMotionMatching(const float DeltaTime, const FAnimationUpdateContext& Context)
{
	bForcePoseSearch = false;
	const float PlayRateAdjustedDeltaTime = DeltaTime * PlaybackRate;
	TimeSinceMotionChosen += PlayRateAdjustedDeltaTime;
	TimeSinceMotionUpdate += PlayRateAdjustedDeltaTime;
	
	FAnimNode_MotionRecorder* MotionRecorderNode = nullptr;
	if (IMotionSnapper* MotionSnapper = Context.GetMessage<IMotionSnapper>())
	{
		MotionRecorderNode = &MotionSnapper->GetNode();
	}
	
	if (MotionRecorderNode)
	{
		ComputeCurrentPose(MotionRecorderNode->GetCurrentPoseArray(MotionRecorderConfigIndex));
	}
	else
	{
		ComputeCurrentPose();
	}

	const TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	bForcePoseSearch = CheckForcePoseSearch(CurrentMotionData);
	
	//Past trajectory mode
	UMotionMatchConfig* MMConfig = CurrentMotionData->MotionMatchConfig;
	if (PastTrajectoryMode == EPastTrajectoryMode::CopyFromCurrentPose)
	{
		int32 CurrentOffset = 0;
		
		for(const TObjectPtr<UMatchFeatureBase> FeaturePtr : MMConfig->Features)
		{
		 	if(FeaturePtr->PoseCategory == EPoseCategory::Responsiveness)
		 	{
		 		const int32 FeatureLimit = FMath::Min(FeaturePtr->Size() + CurrentOffset, InputData.DesiredInputArray.Num());
		 		for(int32 i = CurrentOffset; i < FeatureLimit; ++i)
		 		{
		 			InputData.DesiredInputArray[i] = CurrentInterpolatedPoseArray[i];
		 		}
		 	}
		 	CurrentOffset += FeaturePtr->Size();
		}
	}
	
	if (bForcePoseSearch || TimeSinceMotionUpdate >= UpdateInterval)
	{
		TimeSinceMotionUpdate = 0.0f;
		PoseSearch(Context);
	}
}

void FAnimNode_MSMotionMatching::ComputeCurrentPose()
{
	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	if(!CurrentMotionData)
	{
		UE_LOG(LogTemp, Error, TEXT("MSMotionMatching node cannot compute current pose because the motion data is null"));
		return;
	}
	
	const float PoseInterval = FMath::Max(0.01f, CurrentMotionData->PoseInterval);

	//====== Determine the next dominant pose ========
	float DominantClipLength = GetMotionPlayLength(MMAnimState.AnimId, MMAnimState.AnimType, CurrentMotionData);
	
	float TimePassed = TimeSinceMotionChosen;
	int32 PoseIndex = MMAnimState.StartPoseId;

	//Determine if the new time is out of bounds of the dominant pose clip
	float NewDominantTime = MMAnimState.StartTime + TimePassed;
	if (NewDominantTime >= DominantClipLength)
	{
		if (MMAnimState.bLoop)
		{
			NewDominantTime = FMotionMatchingUtils::WrapAnimationTime(NewDominantTime, DominantClipLength);
		}
		else
		{
			const float TimeToNextClip = DominantClipLength - (TimePassed + MMAnimState.StartTime);

			if (TimeToNextClip < PoseInterval)
			{
				--PoseIndex;
			}

			NewDominantTime = DominantClipLength;
		}

		TimePassed = NewDominantTime - MMAnimState.StartTime;
	}

	int32 NumPosesPassed;
	if (TimePassed < -UE_SMALL_NUMBER)
	{
		NumPosesPassed = FMath::CeilToInt(TimePassed / PoseInterval);
	}
	else
	{
		NumPosesPassed = FMath::FloorToInt(TimePassed / PoseInterval);
	}

	CurrentChosenPoseId = PoseIndex = FMath::Clamp(PoseIndex + NumPosesPassed, 0, CurrentMotionData->Poses.Num() - 1);

	//Get the before and after poses and then interpolate
	FPoseMotionData BeforePose;
	FPoseMotionData AfterPose;

	if (TimePassed < UE_SMALL_NUMBER)
	{
		AfterPose = CurrentMotionData->Poses[PoseIndex];
		BeforePose = CurrentMotionData->Poses[FMath::Clamp(AfterPose.LastPoseId, 0, CurrentMotionData->Poses.Num() - 1)];

		PoseInterpolationValue = 1.0f - FMath::Abs((TimePassed / PoseInterval) - static_cast<float>(NumPosesPassed));
	}
	else
	{
		BeforePose = CurrentMotionData->Poses[FMath::Min(PoseIndex, CurrentMotionData->Poses.Num() - 2)];
		AfterPose = CurrentMotionData->Poses[FMath::Clamp(BeforePose.NextPoseId, 0, CurrentMotionData->Poses.Num() -1)];

		PoseInterpolationValue = (TimePassed / PoseInterval) - static_cast<float>(NumPosesPassed);
	}

	FMotionMatchingUtils::LerpPose(CurrentInterpolatedPose, BeforePose, AfterPose, PoseInterpolationValue);

	const FPoseMatrix& PoseMatrix = CurrentMotionData->LookupPoseMatrix;
	const TArray<float>& PoseArray = PoseMatrix.PoseArray;
	const int32 BeforePoseArrayStartIndex = FMath::Clamp(PoseMatrix.AtomCount * BeforePose.PoseId, 0, PoseArray.Num() - 1);
	const int32 AfterPoseArrayStartIndex = FMath::Clamp(PoseMatrix.AtomCount * AfterPose.PoseId, 0, PoseArray.Num() - 1);
	
	FMotionMatchingUtils::LerpFloatArray(CurrentInterpolatedPoseArray, &PoseArray[BeforePoseArrayStartIndex],
		&PoseArray[AfterPoseArrayStartIndex], PoseInterpolationValue);

	//Inject the input array / trajectory
	if(CurrentInterpolatedPoseArray.Num() > 0)
	{
		CurrentInterpolatedPoseArray[0] = 1.0f;
		
		const int32 MaxCount = FMath::Min(CurrentInterpolatedPoseArray.Num() - 1, InputData.DesiredInputArray.Num());
		for(int32 i = 0; i < MaxCount; ++i)
		{
			CurrentInterpolatedPoseArray[i+1] = InputData.DesiredInputArray[i];
		}
	}
}

void FAnimNode_MSMotionMatching::ComputeCurrentPose(const TArray<float>* CurrentPoseArray)
{
	if(!CurrentPoseArray)
	{
		UE_LOG(LogTemp, Error, TEXT("MSMotionMatching node cannot compute current pose because the CurrentPoseArray is null"));
		return;
	}
	
	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();

	if(!CurrentMotionData)
	{
		UE_LOG(LogTemp, Error, TEXT("MSMotionMatching node cannoe compute current pose because the motion data is null"));
		return;
	}
	
	const float PoseInterval = FMath::Max(0.01f, CurrentMotionData->PoseInterval);
	
	//====== Determine the next dominant pose ========
	float DominantClipLength = GetMotionPlayLength(MMAnimState.AnimId, MMAnimState.AnimType, CurrentMotionData);
	
	float TimePassed = TimeSinceMotionChosen;
	int32 PoseIndex = MMAnimState.StartPoseId;

	//Determine if the new time is out of bounds of the dominant pose clip
	float NewDominantTime = MMAnimState.StartTime + TimePassed;
	if (NewDominantTime >= DominantClipLength)
	{
		if (MMAnimState.bLoop)
		{
			NewDominantTime = FMotionMatchingUtils::WrapAnimationTime(NewDominantTime, DominantClipLength);
		}
		else
		{
			const float TimeToNextClip = DominantClipLength - (TimePassed + MMAnimState.StartTime);

			if (TimeToNextClip < PoseInterval)
			{
				--PoseIndex;
			}

			NewDominantTime = DominantClipLength;
		}

		TimePassed = NewDominantTime - MMAnimState.StartTime;
	}

	int32 NumPosesPassed;
	if (TimePassed < -UE_SMALL_NUMBER)
	{
		NumPosesPassed = FMath::CeilToInt(TimePassed / PoseInterval);
	}
	else
	{
		NumPosesPassed = FMath::FloorToInt(TimePassed / PoseInterval);
	}

	const int32 MaxPoseIndex = CurrentMotionData->Poses.Num() - 1;
	CurrentChosenPoseId = PoseIndex = FMath::Clamp(PoseIndex + NumPosesPassed, 0, MaxPoseIndex);

	//Get the before and after poses and then interpolate
	FPoseMotionData BeforePose;
	FPoseMotionData AfterPose;

	if (TimePassed < -UE_SMALL_NUMBER)
	{
		AfterPose = CurrentMotionData->Poses[PoseIndex];
		BeforePose = CurrentMotionData->Poses[FMath::Clamp(AfterPose.LastPoseId, 0, MaxPoseIndex)];

		PoseInterpolationValue = 1.0f - FMath::Abs((TimePassed / PoseInterval) - static_cast<float>(NumPosesPassed));
	}
	else
	{
		BeforePose = CurrentMotionData->Poses[FMath::Min(PoseIndex, CurrentMotionData->Poses.Num() - 2)];
		AfterPose = CurrentMotionData->Poses[FMath::Clamp(BeforePose.NextPoseId, 0, MaxPoseIndex)];

		PoseInterpolationValue = (TimePassed / PoseInterval) - static_cast<float>(NumPosesPassed);
	}

	PoseInterpolationValue = FMath::Clamp(PoseInterpolationValue, 0.0f, 1.0f);
	
	FMotionMatchingUtils::LerpPose(CurrentInterpolatedPose, BeforePose,
		AfterPose, PoseInterpolationValue);

	const FPoseMatrix& PoseMatrix = CurrentMotionData->LookupPoseMatrix;
	const TArray<float>& PoseArray = PoseMatrix.PoseArray;
	const int32 BeforePoseArrayStartIndex = FMath::Clamp(PoseMatrix.AtomCount * BeforePose.PoseId, 0, PoseArray.Num()-1);
	const int32 AfterPoseArrayStartIndex = FMath::Clamp(PoseMatrix.AtomCount * AfterPose.PoseId, 0, PoseArray.Num()-1);

	FMotionMatchingUtils::LerpFloatArray(CurrentInterpolatedPoseArray, &PoseArray[BeforePoseArrayStartIndex], 
	                                     &PoseArray[AfterPoseArrayStartIndex], PoseInterpolationValue);

#if ENABLE_ANIM_DEBUG && ENABLE_DRAW_DEBUG
	if(CVarMMTrajectoryDebug.GetValueOnAnyThread() == 2)
	{
		DrawChosenInputArrayDebug(AnimInstanceProxy);
	}
#endif

	//Inject the input array / trajectory. This currently assumes that all input is first in the pose array
	CurrentInterpolatedPoseArray[0] = 1.0f;
	const int32 Iterations = FMath::Min(CurrentInterpolatedPoseArray.Num() - 1, InputData.DesiredInputArray.Num());
	for(int32 i = 0; i < Iterations; ++i)
	{
		CurrentInterpolatedPoseArray[i+1] = InputData.DesiredInputArray[i]; //+1 to pose array to make room for Pose Cost Multiplier value
	}

	int32 FeatureOffset = 0;
	for(const TObjectPtr<UMatchFeatureBase> Feature : CurrentMotionData->MotionMatchConfig->Features)
	{
		const int32 FeatureSize = Feature->Size();
		
		if(Feature->IsMotionSnapshotCompatible())
		{
			for(int32 i = 0; i < FeatureSize; ++i)
			{
				CurrentInterpolatedPoseArray[FeatureOffset + i + 1] = (*CurrentPoseArray)[FeatureOffset + i];
			}
		}

		FeatureOffset += FeatureSize;
	}
}

void FAnimNode_MSMotionMatching::PoseSearch(const FAnimationUpdateContext& Context)
{
	if (bBlendInputResponse)
	{
		ApplyTrajectoryBlending();
	}

	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	if(!CurrentMotionData)
	{
		return;
	}
	
	const int32 MaxPoseId = CurrentMotionData->Poses.Num() - 1;
	CurrentChosenPoseId = FMath::Clamp(CurrentChosenPoseId, 0, MaxPoseId);
	int32 NextPoseId = CurrentMotionData->Poses[CurrentChosenPoseId].NextPoseId;
	if(NextPoseId < 0)
	{
		NextPoseId = CurrentChosenPoseId;
	}

	const FPoseMotionData& NextPose = CurrentMotionData->Poses[FMath::Clamp(NextPoseId, 0, MaxPoseId)];

	if (!bForcePoseSearch && bEnableToleranceTest)
	{
		if (NextPoseToleranceTest(NextPose))
		{
			TimeSinceMotionUpdate = 0.0f;
			return;
		}
	}

	const int32 LowestPoseId = (SearchQuality == EMotionMatchingSearchQuality::Performance) 
		? GetLowestCostPoseId_Standard()
		: GetLowestCostPoseId_HighQuality(Context.GetDeltaTime());

	const FPoseMotionData& BestPose = CurrentMotionData->Poses[LowestPoseId];

	/*Here we are checking if the chosen pose is at or very close to the same pose that is currently playing.
	 * If it is, then there is no need to pose transition, just keep playing the animation. There are several criteria.
	 * Firstly the pose must be the same animation and mirror (animId, AnimType and bMirror). If the first condition
	 * is met then the animation either needs to be looping or the pose must be within 'SamePoseTolerance' seconds
	 * of the current pose to be considered the same. For blend spaces there is an additional criteria.
	 */
	const bool bSameAnim = BestPose.AnimId == CurrentInterpolatedPose.AnimId
					&& BestPose.AnimType == CurrentInterpolatedPose.AnimType
					&& BestPose.bMirrored == CurrentInterpolatedPose.bMirrored;

	TObjectPtr<const UMotionAnimObject> SourceMotion = MotionData->GetSourceAnim(BestPose.AnimId, BestPose.AnimType);
	
	const bool bWinnerAtSameLocation = bSameAnim && ((SourceMotion ? SourceMotion->bLoop : false) ||
									(FMath::Abs(BestPose.Time - CurrentInterpolatedPose.Time) < SamePoseTolerance
									&& FVector2D::DistSquared(BestPose.BlendSpacePosition, CurrentInterpolatedPose.BlendSpacePosition) < 1.0f));
	
	if (!bWinnerAtSameLocation)
	{
		TransitionToPose(BestPose.PoseId, Context);
	}
}

void FAnimNode_MSMotionMatching::TransitionPoseSearch(const FAnimationUpdateContext& Context)
{
	TransitionToPose(GetLowestCostPoseId_Transition(), Context, 0.0f);
}

bool FAnimNode_MSMotionMatching::CheckForcePoseSearch(const UMotionDataAsset* InMotionData) const
{
	if(!InMotionData)
	{
		return false;
	}
	
	if(bUserForcePoseSearch
		|| CurrentInterpolatedPose.SearchFlag == EPoseSearchFlag::DoNotUse
		|| !CurrentInterpolatedPose.MotionTags.HasAllExact(RequiredMotionTags))
	{
		return true;
	}
	
	if (!MMAnimState.bLoop)
	{
		if (MMAnimState.StartTime + TimeSinceMotionChosen  > MMAnimState.AnimLength)
		{
			return true;
		}
	}
	const int32 PoseCountToCheck = CurrentInterpolatedPose.PoseId + FMath::CeilToInt32(BlendTime / InMotionData->GetPoseInterval());

	//End of pose data, pose search must be forced
	if(PoseCountToCheck >= InMotionData->Poses.Num())
	{
		return true;
	}

	//Check ahead to see if there will be a DoNotUse pose within the blend time or a new animation
	for(int32 i = CurrentInterpolatedPose.PoseId; i < PoseCountToCheck; ++i)
	{
		const FPoseMotionData& ThisPose = InMotionData->Poses[i];
		
		if(ThisPose.SearchFlag == EPoseSearchFlag::DoNotUse
			|| ThisPose.AnimId != CurrentInterpolatedPose.AnimId
			|| ThisPose.AnimType != CurrentInterpolatedPose.AnimType)
		{
			return true;
		}
	}
	
	return false;
}

/** TRANSITION POSE SEARCH*/
int32 FAnimNode_MSMotionMatching::GetLowestCostPoseId_Transition()
{
	if(!GenerateCalibrationArray())
	{
		return CurrentChosenPoseId;
	}
	
	//Cannot do Current Pose or Next Natural search because this function is only used for transitioning into motion matching
	//where there is no known pose.
	
	//Main Loop Search
	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	// int32 PoseCount = CurrentMotionData->SearchPoseMatrix.PoseCount;
	const int32 AtomCount = CurrentMotionData->SearchPoseMatrix.AtomCount;
	
	int32 LowestPoseId_SM = 0;
	float LowestCost = 10000000.0f;

	int32 MotionTagStartPoseIndex;
	int32 MotionTagEndPoseIndex;
	CurrentMotionData->FindMotionTagRangeIndices(RequiredMotionTags, MotionTagStartPoseIndex, MotionTagEndPoseIndex);
	const int32 OuterAABBStartIndex = FMath::FloorToInt32(MotionTagStartPoseIndex / 64.0f);
	const int32 OuterAABBEndIndex = FMath::CeilToInt32(MotionTagEndPoseIndex / 64.0f);
	const TArray<float>& OuterAABBArray = CurrentMotionData->PoseAABBMatrix_Outer.ExtentsArray;
	const TArray<float>& InnerAABBArray = CurrentMotionData->PoseAABBMatrix_Inner.ExtentsArray;
	const TArray<float>& PoseArray = CurrentMotionData->SearchPoseMatrix.PoseArray;
	for(int32 OuterAABBIndex = OuterAABBStartIndex; OuterAABBIndex < OuterAABBEndIndex; ++OuterAABBIndex)
	{
		const int32 OuterAABBAtomStartIndex = OuterAABBIndex * AtomCount * 2;
		
		float AABBCost = 0.0f;
		for(int32 DimIndex = 1; DimIndex < AtomCount; ++DimIndex)
		{
			const int32 OuterAABBAtomIndex = OuterAABBAtomStartIndex + (DimIndex * 2);

			const float ClosestPoint = FMath::Clamp(CurrentInterpolatedPoseArray[DimIndex],
			                                        OuterAABBArray[OuterAABBAtomIndex],
			                                        OuterAABBArray[OuterAABBAtomIndex + 1]);

			AABBCost += FMath::Abs(CurrentInterpolatedPoseArray[DimIndex] - ClosestPoint) * CalibrationArray[DimIndex - 1];
		}

		if(AABBCost < LowestCost)
		{
			//We need to search the inner AABBs
			const int32 InnerAABBStartIndex = OuterAABBIndex * 4;
			const int32 InnerAABBEndIndex = FMath::Min(InnerAABBStartIndex + 4, FMath::CeilToInt32(MotionTagEndPoseIndex / 16.0f));
			for(int32 InnerAABBIndex = InnerAABBStartIndex; InnerAABBIndex < InnerAABBEndIndex; ++InnerAABBIndex)
			{
				const int32 InnerAABBAtomStartIndex = InnerAABBIndex * AtomCount * 2;

				AABBCost = 0.0f;
				for(int32 DimIndex = 1; DimIndex < AtomCount; ++DimIndex)
				{
					const int32 InnerAABBAtomIndex = InnerAABBAtomStartIndex + (DimIndex * 2);

					const float ClosestPoint = FMath::Clamp(CurrentInterpolatedPoseArray[DimIndex],
															InnerAABBArray[InnerAABBAtomIndex],
															InnerAABBArray[InnerAABBAtomIndex + 1]);

					AABBCost += FMath::Abs(CurrentInterpolatedPoseArray[DimIndex] - ClosestPoint) * CalibrationArray[DimIndex - 1];
				}

				if(AABBCost < LowestCost)
				{
					const int32 StartPoseIndex = FMath::Max(InnerAABBIndex * 16, MotionTagStartPoseIndex);
					const int32 EndPoseIndex = FMath::Min((InnerAABBIndex * 16) + 16, MotionTagEndPoseIndex);
					for(int32 PoseIndex = StartPoseIndex; PoseIndex < EndPoseIndex; ++PoseIndex)
					{
						float Cost = 0.0f;
						const int32 MatrixStartIndex = PoseIndex * AtomCount;
						const float PoseFavour = PoseArray[MatrixStartIndex]; //Pose cost multiplier is the first atom of a pose array
						for(int32 AtomIndex = 1; AtomIndex < AtomCount; ++AtomIndex)
						{
							Cost += FMath::Abs(PoseArray[MatrixStartIndex + AtomIndex] - CurrentInterpolatedPoseArray[AtomIndex])
									* CalibrationArray[AtomIndex - 1] * PoseFavour; 
						}
						
						if(Cost < LowestCost)
						{
							LowestCost = Cost;
							LowestPoseId_SM = PoseIndex;
						}
					}
				}
			}
		}
	}

	return CurrentMotionData->MatrixPoseIdToDatabasePoseId(LowestPoseId_SM);
}

/** STANDARD QUALITY POSE SEARCH*/
int32 FAnimNode_MSMotionMatching::GetLowestCostPoseId_Standard()
{
	if(!GenerateCalibrationArray())
	{
		return CurrentChosenPoseId;
	}

	bool bNextNaturalChosen = false;

	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	const int32 AtomCount = CurrentMotionData->SearchPoseMatrix.AtomCount;
	const TArray<float>& LookupPoseArray = CurrentMotionData->LookupPoseMatrix.PoseArray;
	
	//Check cost of current pose first for "Favour Current Pose"
	int32 LowestPoseId_LM = 0; //_LM stands for Lookup Matrix
	int32 LowestPoseId_SM = 0; //_SM stands for Search Matrix
	float LowestCost = UE_MAX_FLT;
	if(!bForcePoseSearch)
	{
		if(bFavourCurrentPose)
		{
			LowestPoseId_LM = CurrentInterpolatedPose.PoseId;
			const int32 PoseStartIndex = LowestPoseId_LM * AtomCount;
			const float PoseFavour = LookupPoseArray[PoseStartIndex]; //Pose cost multiplier is the first atom of a pose array

			for(int32 AtomIndex = 1; AtomIndex < AtomCount; ++AtomIndex)
			{
				LowestCost += FMath::Abs(LookupPoseArray[PoseStartIndex + AtomIndex] - CurrentInterpolatedPoseArray[AtomIndex])
					* CalibrationArray[AtomIndex - 1];
			}
			LowestCost *= PoseFavour;
			LowestCost *= CurrentPoseFavour;
		}

		//Next Natural
		bNextNaturalChosen = true;
		LowestPoseId_LM = GetLowestCostNextNaturalId(CurrentInterpolatedPose.PoseId, LowestCost, CurrentMotionData); //The returned pose id is in lookup matrix space
	
		if(bNextNaturalToleranceTest)
		{
			const FPoseMotionData& NextNaturalPose = CurrentMotionData->Poses[LowestPoseId_LM];
		
			if(NextPoseToleranceTest(NextNaturalPose))
			{
				return LowestPoseId_LM;
			}
		}
	
		LowestPoseId_SM = CurrentMotionData->DatabasePoseIdToMatrixPoseId(LowestPoseId_LM);
	}

#if WITH_EDITORONLY_DATA		
	PosesChecked = InnerAABBsChecked = InnerAABBsPassed = OuterAABBsChecked = OuterAABBsPassed = 0;
#endif
	
	//First go through the OuterAABBs
	int32 MotionTagStartPoseIndex;
	int32 MotionTagEndPoseIndex;
	CurrentMotionData->GetMotionTagStartAndEndPoseIndex(RequiredMotionTags, MotionTagStartPoseIndex, MotionTagEndPoseIndex);
	
	const int32 OuterAABBStartIndex = FMath::FloorToInt32(MotionTagStartPoseIndex / 64.0f);
	const int32 OuterAABBEndIndex = FMath::CeilToInt32(MotionTagEndPoseIndex / 64.0f);
	const TArray<float>& OuterAABBArray = CurrentMotionData->PoseAABBMatrix_Outer.ExtentsArray;
	const TArray<float>& InnerAABBArray = CurrentMotionData->PoseAABBMatrix_Inner.ExtentsArray;
	const TArray<float>& PoseArray = CurrentMotionData->SearchPoseMatrix.PoseArray;
	for(int32 OuterAABBIndex = OuterAABBStartIndex; OuterAABBIndex < OuterAABBEndIndex; ++OuterAABBIndex)
	{
#if WITH_EDITORONLY_DATA	
		++OuterAABBsChecked;
#endif
		
		const int32 OuterAABBAtomStartIndex = OuterAABBIndex * AtomCount * 2;
		
		float AABBCost = 0.0f;
		for(int32 DimIndex = 1; DimIndex < AtomCount; ++DimIndex)
		{
			const int32 OuterAABBAtomIndex = OuterAABBAtomStartIndex + (DimIndex * 2);

			const float ClosestPoint = FMath::Clamp(CurrentInterpolatedPoseArray[DimIndex],
			                                        OuterAABBArray[OuterAABBAtomIndex],
			                                        OuterAABBArray[OuterAABBAtomIndex + 1]);

			AABBCost += FMath::Abs(CurrentInterpolatedPoseArray[DimIndex] - ClosestPoint) * CalibrationArray[DimIndex - 1];
		}

		if(AABBCost < LowestCost)
		{
#if WITH_EDITORONLY_DATA	
			++OuterAABBsPassed;
#endif
			
			//We need to search the inner AABBs

			const int32 InnerAABBStartIndex = OuterAABBIndex * 4;
			const int32 InnerAABBEndIndex = FMath::Min(InnerAABBStartIndex + 4, FMath::CeilToInt32(MotionTagEndPoseIndex / 16.0f));
			for(int32 InnerAABBIndex = InnerAABBStartIndex; InnerAABBIndex < InnerAABBEndIndex; ++InnerAABBIndex)
			{
#if WITH_EDITORONLY_DATA	
				++InnerAABBsChecked;
#endif
				
				const int32 InnerAABBAtomStartIndex = InnerAABBIndex * AtomCount * 2;

				AABBCost = 0.0f;
				for(int32 DimIndex = 1; DimIndex < AtomCount; ++DimIndex)
				{
					const int32 InnerAABBAtomIndex = InnerAABBAtomStartIndex + (DimIndex * 2);

					const float ClosestPoint = FMath::Clamp(CurrentInterpolatedPoseArray[DimIndex],
															InnerAABBArray[InnerAABBAtomIndex],
															InnerAABBArray[InnerAABBAtomIndex + 1]);

					AABBCost += FMath::Abs(CurrentInterpolatedPoseArray[DimIndex] - ClosestPoint) * CalibrationArray[DimIndex - 1];
				}

				if(AABBCost < LowestCost)
				{
#if WITH_EDITORONLY_DATA	
					++InnerAABBsPassed;
#endif
					
					const int32 StartPoseIndex = FMath::Max(InnerAABBIndex * 16, MotionTagStartPoseIndex);
					const int32 EndPoseIndex = FMath::Min((InnerAABBIndex * 16) + 16, MotionTagEndPoseIndex);
					for(int32 PoseIndex = StartPoseIndex; PoseIndex < EndPoseIndex; ++PoseIndex)
					{
#if WITH_EDITORONLY_DATA	
						++PosesChecked;
#endif
						
						float Cost = 0.0f;
						const int32 MatrixStartIndex = PoseIndex * AtomCount;
						const float PoseFavour = PoseArray[MatrixStartIndex]; //Pose cost multiplier is the first atom of a pose array
						
						for(int32 AtomIndex = 1; AtomIndex < AtomCount; ++AtomIndex)
						{
							Cost += FMath::Abs(PoseArray[MatrixStartIndex + AtomIndex] - CurrentInterpolatedPoseArray[AtomIndex])
									* CalibrationArray[AtomIndex - 1]; 
						}
						
						Cost *= PoseFavour;
						if(Cost < LowestCost)
						{
							bNextNaturalChosen = false;
							LowestCost = Cost;
							LowestPoseId_SM = PoseIndex;
						}
					}
				}
			}
		}
	}

#if WITH_EDITORONLY_DATA
	RecordHistoricalPoseSearch(PosesChecked);
#endif

	if(bNextNaturalChosen)
	{
		return LowestPoseId_LM;
	}
	else
	{
		return CurrentMotionData->MatrixPoseIdToDatabasePoseId(LowestPoseId_SM);
	}
}

/** HIGH QUALITY POSE SEARCH*/
int32 FAnimNode_MSMotionMatching::GetLowestCostPoseId_HighQuality(const float DeltaTime)
{
	if(!GenerateCalibrationArray())
	{
		return CurrentChosenPoseId;
	}

	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	const int32 AtomCount = CurrentMotionData->SearchPoseMatrix.AtomCount;
	const TArray<float>& LookupPoseArray = CurrentMotionData->LookupPoseMatrix.PoseArray;
	
	//Check cost of current pose first for "Favour Current Pose"
	int32 LowestPoseId_LM = 0; //_LM stands for Lookup Matrix, _SM stands for Search Matrix
	float LowestCost = 10000000.0f;
	if(bFavourCurrentPose && !bForcePoseSearch)
	{
		LowestPoseId_LM = CurrentInterpolatedPose.PoseId;
		const int32 PoseStartIndex = LowestPoseId_LM * AtomCount;
		const float PoseFavour = LookupPoseArray[PoseStartIndex]; //Pose cost multiplier is the first atom of a pose array

		for(int32 AtomIndex = 1; AtomIndex < AtomCount; ++AtomIndex)
		{
			LowestCost += FMath::Abs(LookupPoseArray[PoseStartIndex + AtomIndex] - CurrentInterpolatedPoseArray[AtomIndex])
				* CalibrationArray[AtomIndex - 1];
		}
		LowestCost *= PoseFavour;
		LowestCost *= CurrentPoseFavour;
	}

	//Next Natural
	bool bNextNaturalChosen = true;
	LowestPoseId_LM = GetLowestCostNextNaturalId(CurrentInterpolatedPose.PoseId, LowestCost, CurrentMotionData); //The returned pose id is in lookup matrix space
	
	if(bNextNaturalToleranceTest)
	{
		const FPoseMotionData& NextNaturalPose = CurrentMotionData->Poses[LowestPoseId_LM];
		
		if(NextPoseToleranceTest(NextNaturalPose))
		{
			return LowestPoseId_LM;
		}
	}
	
	int32 LowestPoseId_SM = CurrentMotionData->DatabasePoseIdToMatrixPoseId(LowestPoseId_LM);

#if WITH_EDITORONLY_DATA		
	PosesChecked = InnerAABBsChecked = InnerAABBsPassed = OuterAABBsChecked = OuterAABBsPassed = 0;
#endif
	
	//First go through the OuterAABBs
	const int32 MotionTagStartPoseIndex = CurrentMotionData->GetMotionTagStartPoseIndex(RequiredMotionTags); //Todo: Combine these two lines into 1
	const int32 MotionTagEndPoseIndex = CurrentMotionData->GetMotionTagEndPoseIndex(RequiredMotionTags);
	const int32 OuterAABBStartIndex = FMath::FloorToInt32(MotionTagStartPoseIndex / 64.0f);
	const int32 OuterAABBEndIndex = FMath::CeilToInt32(MotionTagEndPoseIndex / 64.0f);
	const TArray<float>& OuterAABBArray = CurrentMotionData->PoseAABBMatrix_Outer.ExtentsArray;
	const TArray<float>& InnerAABBArray = CurrentMotionData->PoseAABBMatrix_Inner.ExtentsArray;
	const TArray<float>& PoseArray = CurrentMotionData->SearchPoseMatrix.PoseArray;
	for(int32 OuterAABBIndex = OuterAABBStartIndex; OuterAABBIndex < OuterAABBEndIndex; ++OuterAABBIndex)
	{
#if WITH_EDITORONLY_DATA	
		++OuterAABBsChecked;
#endif
		
		const int32 OuterAABBAtomStartIndex = OuterAABBIndex * AtomCount * 2;
		
		float AABBCost = 0.0f;
		for(int32 DimIndex = 1; DimIndex < AtomCount; ++DimIndex)
		{
			const int32 OuterAABBAtomIndex = OuterAABBAtomStartIndex + (DimIndex * 2);

			const float ClosestPoint = FMath::Clamp(CurrentInterpolatedPoseArray[DimIndex],
			                                        OuterAABBArray[OuterAABBAtomIndex],
			                                        OuterAABBArray[OuterAABBAtomIndex + 1]);

			AABBCost += FMath::Abs(CurrentInterpolatedPoseArray[DimIndex] - ClosestPoint) * CalibrationArray[DimIndex - 1];
		}

		if(AABBCost < LowestCost)
		{
#if WITH_EDITORONLY_DATA	
			++OuterAABBsPassed;
#endif
			
			//We need to search the inner AABBs

			const int32 InnerAABBStartIndex = OuterAABBIndex * 4;
			const int32 InnerAABBEndIndex = FMath::Min(InnerAABBStartIndex + 4, FMath::CeilToInt32(MotionTagEndPoseIndex / 16.0f));
			for(int32 InnerAABBIndex = InnerAABBStartIndex; InnerAABBIndex < InnerAABBEndIndex; ++InnerAABBIndex)
			{
#if WITH_EDITORONLY_DATA	
				++InnerAABBsChecked;
#endif
				
				const int32 InnerAABBAtomStartIndex = InnerAABBIndex * AtomCount * 2;

				AABBCost = 0.0f;
				for(int32 DimIndex = 1; DimIndex < AtomCount; ++DimIndex)
				{
					const int32 InnerAABBAtomIndex = InnerAABBAtomStartIndex + (DimIndex * 2);

					const float ClosestPoint = FMath::Clamp(CurrentInterpolatedPoseArray[DimIndex],
															InnerAABBArray[InnerAABBAtomIndex],
															InnerAABBArray[InnerAABBAtomIndex + 1]);

					AABBCost += FMath::Abs(CurrentInterpolatedPoseArray[DimIndex] - ClosestPoint) * CalibrationArray[DimIndex - 1];
				}

				if(AABBCost < LowestCost)
				{
#if WITH_EDITORONLY_DATA	
					++InnerAABBsPassed;
#endif
					const float PoseInterval = CurrentMotionData->PoseInterval;
					const float ResVelWeight = CurrentMotionData->MotionMatchConfig->ResultantVelocityWeight;
					const int32 ResIndex = AtomCount - 12;
					
					const int32 StartPoseIndex = FMath::Max(InnerAABBIndex * 16, MotionTagStartPoseIndex);
					const int32 EndPoseIndex = FMath::Min((InnerAABBIndex * 16) + 16, MotionTagEndPoseIndex);
					for(int32 PoseIndex = StartPoseIndex; PoseIndex < EndPoseIndex; ++PoseIndex)
					{
#if WITH_EDITORONLY_DATA	
						++PosesChecked;
#endif
						
						float Cost = 0.0f;
						const int32 MatrixStartIndex = PoseIndex * AtomCount;
						const float PoseFavour = PoseArray[MatrixStartIndex]; //Pose cost multiplier is the first atom of a pose array

						/** Basic Cost Loop*/
						for(int32 AtomIndex = 1; AtomIndex < AtomCount; ++AtomIndex)
						{
							Cost += FMath::Abs(PoseArray[MatrixStartIndex + AtomIndex] - CurrentInterpolatedPoseArray[AtomIndex])
									* CalibrationArray[AtomIndex - 1]; 
						}
						

						/** High Quality Cost Loop (I.e. Resultant Velocity Costing */
						float ResVelX = CurrentInterpolatedPoseArray[ResIndex] - PoseArray[MatrixStartIndex + ResIndex] / DeltaTime;
						float ResVelY = CurrentInterpolatedPoseArray[ResIndex+1] - PoseArray[MatrixStartIndex + ResIndex+1] / DeltaTime;
						float ResVelZ = CurrentInterpolatedPoseArray[ResIndex+2] - PoseArray[MatrixStartIndex + ResIndex+2] / DeltaTime;

						float ResVelCost = FMath::Abs(ResVelX - CurrentInterpolatedPoseArray[ResIndex + 3]) * CalibrationArray[ResIndex + 2];
						ResVelCost += FMath::Abs(ResVelY - CurrentInterpolatedPoseArray[ResIndex + 4]) * CalibrationArray[ResIndex + 3];
						ResVelCost +=	FMath::Abs(ResVelZ - CurrentInterpolatedPoseArray[ResIndex + 5]) * CalibrationArray[ResIndex + 4];

						ResVelX = CurrentInterpolatedPoseArray[ResIndex+6] - PoseArray[MatrixStartIndex + ResIndex+6] / DeltaTime;
						ResVelY = CurrentInterpolatedPoseArray[ResIndex+7] - PoseArray[MatrixStartIndex + ResIndex+7] / DeltaTime;
						ResVelZ = CurrentInterpolatedPoseArray[ResIndex+8] - PoseArray[MatrixStartIndex + ResIndex+8] / DeltaTime;

						ResVelCost += FMath::Abs(ResVelX - CurrentInterpolatedPoseArray[ResIndex + 9]) * CalibrationArray[ResIndex + 8];
						ResVelCost += FMath::Abs(ResVelY - CurrentInterpolatedPoseArray[ResIndex + 10]) * CalibrationArray[ResIndex + 9];
						ResVelCost +=	FMath::Abs(ResVelZ - CurrentInterpolatedPoseArray[ResIndex + 11]) * CalibrationArray[ResIndex + 10];

						Cost += ResVelCost * ResVelWeight;
						Cost *= PoseFavour;
						if(Cost < LowestCost)
						{
							bNextNaturalChosen = false;
							LowestCost = Cost;
							LowestPoseId_SM = PoseIndex;
						}
					}
				}
			}
		}
	}

#if WITH_EDITORONLY_DATA
	RecordHistoricalPoseSearch(PosesChecked);
#endif

	if(bNextNaturalChosen)
	{
		return LowestPoseId_LM;
	}
	else
	{
		return CurrentMotionData->MatrixPoseIdToDatabasePoseId(LowestPoseId_SM);
	}
}

int32 FAnimNode_MSMotionMatching::GetLowestCostNextNaturalId(int32 LowestPoseId_LM, float& OutLowestCost, TObjectPtr<const UMotionDataAsset> InMotionData)
{
	//Determine how many valid next naturals there are
	const int32 NextNaturalStart = CurrentInterpolatedPose.PoseId;
	const int32 NextNaturalEnd = FMath::Clamp(CurrentInterpolatedPose.PoseId + FMath::CeilToInt32(NextNaturalRange
		/ InMotionData->PoseInterval), 0, InMotionData->Poses.Num() - 1);
	const int32 CurrentAnimId = CurrentInterpolatedPose.AnimId;
	const EMotionAnimAssetType CurrentAnimType = CurrentInterpolatedPose.AnimType;

	int32 ValidNextNaturalCount = 0;
	for(int32 i = NextNaturalStart; i < NextNaturalEnd; ++i)
	{
		const FPoseMotionData& Pose = InMotionData->Poses[i];
		
		if(Pose.SearchFlag == EPoseSearchFlag::DoNotUse
			|| Pose.AnimId != CurrentAnimId
			|| Pose.AnimType != CurrentAnimType)
		{
			break;
		}

		++ValidNextNaturalCount;
	}
	
	const int32 AtomCount = InMotionData->LookupPoseMatrix.AtomCount;
	const TArray<float>& LookupPoseArray = InMotionData->LookupPoseMatrix.PoseArray;

	const float FinalNextNaturalFavour = bFavourNextNatural ? NextNaturalFavour : 1.0f;

	//Search next naturals and determine the lowest cost one.
	for(int32 PoseIndex = NextNaturalStart; PoseIndex < NextNaturalStart + ValidNextNaturalCount; ++PoseIndex)
	{
		float Cost = 0.0f;
		const int32 MatrixStartIndex = PoseIndex * AtomCount;
		for(int32 AtomIndex = 1; AtomIndex < AtomCount; ++AtomIndex)
		{
			Cost += FMath::Abs(LookupPoseArray[MatrixStartIndex + AtomIndex] - CurrentInterpolatedPoseArray[AtomIndex])
				* CalibrationArray[AtomIndex - 1];
		}

		Cost *= LookupPoseArray[MatrixStartIndex] * FinalNextNaturalFavour;
		if(Cost < OutLowestCost)
		{
			OutLowestCost = Cost;
			LowestPoseId_LM = PoseIndex;
		}
	}

	return LowestPoseId_LM;
}

void FAnimNode_MSMotionMatching::TransitionToPose(const int32 PoseId, const FAnimationUpdateContext& Context, const float TimeOffset /*= 0.0f*/)
{
	switch (TransitionMethod)
	{
		case ETransitionMethod::None: { JumpToPose(PoseId, TimeOffset); } break;
		case ETransitionMethod::Inertialization:
		{
			JumpToPose(PoseId, TimeOffset);
				
			UE::Anim::IInertializationRequester* InertializationRequester = Context.GetMessage<UE::Anim::IInertializationRequester>();
			if (InertializationRequester)
			{
				InertializationRequester->RequestInertialization(BlendTime);
				InertializationRequester->AddDebugRecord(*Context.AnimInstanceProxy, Context.GetCurrentNodeId());
			}
			else
			{
				//FAnimNode_Inertialization::LogRequestError(Context, BlendPose[ChildIndex]);
				UE_LOG(LogTemp, Error, TEXT("Motion Matching Node: Failed to get inertialisation node ancestor in the animation graph. Either add an inertialiation node or change the blend type."));
			}

		} break;
	}
}

void FAnimNode_MSMotionMatching::JumpToPose(const int32 PoseIdDatabase, const float TimeOffset /*= 0.0f */)
{
	TimeSinceMotionChosen = TimeSinceMotionUpdate;
	CurrentChosenPoseId = PoseIdDatabase;

	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	const FPoseMotionData& Pose = CurrentMotionData->Poses[PoseIdDatabase];

	switch (Pose.AnimType)
	{
		//Sequence Pose
		case EMotionAnimAssetType::Sequence: 
		{
			TObjectPtr<const UMotionSequenceObject> MotionAnim = CurrentMotionData->GetSourceSequenceAtIndex(Pose.AnimId);

			if (!MotionAnim || !MotionAnim->Sequence)
			{
				break;
			}

			MMAnimState = FAnimChannelState(Pose, MotionAnim->Sequence->GetPlayLength(), MotionAnim->bLoop,
				MotionAnim->PlayRate, Pose.bMirrored, TimeSinceMotionChosen, TimeOffset);

		} break;
		//Blend Space Pose
		case EMotionAnimAssetType::BlendSpace:
		{
			TObjectPtr<const UMotionBlendSpaceObject> MotionBlendSpace = CurrentMotionData->GetSourceBlendSpaceAtIndex(Pose.AnimId);

			if (!MotionBlendSpace || !MotionBlendSpace->BlendSpace)
			{
				break;
			}

			MMAnimState = FAnimChannelState(Pose, MotionBlendSpace->GetPlayLength(), MotionBlendSpace->bLoop,
				MotionBlendSpace->PlayRate, Pose.bMirrored, TimeSinceMotionChosen, TimeOffset);
				
			MotionBlendSpace->BlendSpace->GetSamplesFromBlendInput(FVector(
				Pose.BlendSpacePosition.X, Pose.BlendSpacePosition.Y, 0.0f),
				MMAnimState.BlendSampleDataCache, MMAnimState.CachedTriangulationIndex, false);
				
		} break;
		//Composites
		case EMotionAnimAssetType::Composite:
		{
			TObjectPtr<const UMotionCompositeObject> MotionComposite = CurrentMotionData->GetSourceCompositeAtIndex(Pose.AnimId);

			if (!MotionComposite || !MotionComposite->AnimComposite)
			{
				break;
			}

			MMAnimState = FAnimChannelState(Pose, MotionComposite->AnimComposite->GetPlayLength(),
				MotionComposite->bLoop, MotionComposite->PlayRate, Pose.bMirrored, TimeSinceMotionChosen, TimeOffset);
		} break;
		default: ; 
	}
}

TObjectPtr<const UMotionDataAsset> FAnimNode_MSMotionMatching::GetMotionData() const
{
	return MotionData;
	//return GET_ANIM_NODE_DATA(TObjectPtr<UMotionDataAsset>, MotionData);
}

TObjectPtr<const UMotionCalibration> FAnimNode_MSMotionMatching::GetUserCalibration() const
{
	return UserCalibration;
	//return GET_ANIM_NODE_DATA(TObjectPtr<UMotionCalibration>, UserCalibration);
}

UMirrorDataTable* FAnimNode_MSMotionMatching::GetMirrorDataTable() const
{
	if(TObjectPtr<const UMotionDataAsset> ThisMotionData = GetMotionData())
	{
		return ThisMotionData->MirrorDataTable;
	}

	return nullptr;
}

void FAnimNode_MSMotionMatching::CheckValidToEvaluate(const FAnimInstanceProxy* InAnimInstanceProxy)
{
	bValidToEvaluate = true;
	
	//Validate Motion Data
	TObjectPtr<UMotionDataAsset> CurrentMotionData = MotionData;
	if (!CurrentMotionData)
	{
		UE_LOG(LogTemp, Warning, TEXT("Motion matching node failed to initialize. Motion Data has not been set."))
		bValidToEvaluate = false;
		return;
	}
	
	if(!CurrentMotionData->IsSearchPoseMatrixGenerated())
	{
		CurrentMotionData->GenerateSearchPoseMatrix();
	}

	//Validate Motion Matching Configuration
	//Todo: Move this somewhere else maybe?
	UMotionMatchConfig* MMConfig = CurrentMotionData->MotionMatchConfig;
	if (MMConfig)
	{
		const int32 PoseArraySize = CurrentMotionData->SearchPoseMatrix.AtomCount;

		CurrentInterpolatedPose = FPoseMotionData();
		CurrentInterpolatedPoseArray.Empty(PoseArraySize + 1);
		CalibrationArray.Empty(PoseArraySize + 1);

		CurrentInterpolatedPoseArray.SetNumZeroed(PoseArraySize);
		InputData.DesiredInputArray.SetNumZeroed(PoseArraySize);
		CalibrationArray.SetNumZeroed(PoseArraySize);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Motion matching node failed to initialize. Motion Match Config has not been set on the motion matching node."));
		bValidToEvaluate = false;
		return;
	}

	if(MMConfig->TotalDimensionCount != CurrentMotionData->SearchPoseMatrix.AtomCount - 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Motion matching node failed to initialize. Motion Match Config does not have the same number of atoms as the search pose matrix."
								"Did you forget to re-process the motion data after making changes?"));
		bValidToEvaluate = false;
		return;
	}

	//Validate MMConfig matches internal calibration (i.e. the MMConfig has not been since changed)
	for(auto& MotionCalibration : CurrentMotionData->FeatureStandardDeviations)
	{
		if (!MotionCalibration.IsValidWithConfig(MMConfig))
		{
			UE_LOG(LogTemp, Warning, TEXT("Motion matching node failed to initialize. Internal calibration sets atom count does not match the motion config. Did you change the motion config and forget to pre-process?"));
			bValidToEvaluate = false;
			return;
		}
	}

	FinalCalibrationSets.Empty(CurrentMotionData->FeatureStandardDeviations.Num() + 1);
	for (auto& FeatureStdDev : CurrentMotionData->FeatureStandardDeviations)
	{
		FinalCalibrationSets.Emplace(FCalibrationData());
		FinalCalibrationSets.Last().GenerateFinalWeights(MMConfig, FeatureStdDev);
	}
	
	if (UserCalibration)
	{
		UserCalibration->ValidateData(MMConfig, false);
	}

	JumpToPose(0);
	if (const UAnimSequenceBase* Sequence = GetPrimaryAnim())
	{
		InternalTimeAccumulator = FMath::Clamp(MMAnimState.AnimTime, 0.0f, Sequence->GetPlayLength());

		if (PlaybackRate * Sequence->RateScale < 0.0f)
		{
			InternalTimeAccumulator = Sequence->GetPlayLength();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Motion matching node failed to initialize. The starting sequence is null. Check that all animations in the MotionData are valid"));
		bValidToEvaluate = false;
	}
}

float FAnimNode_MSMotionMatching::GetMotionPlayLength(const int32 AnimId, const EMotionAnimAssetType AnimType, TObjectPtr<const UMotionDataAsset> InMotionData)
{
	if(!InMotionData)
	{
		return 0.0f;
	}
	
	switch (MMAnimState.AnimType)
	{
	case EMotionAnimAssetType::Sequence:
		{
			if(TObjectPtr<const UMotionSequenceObject> SourceSequence = InMotionData->GetSourceSequenceAtIndex(MMAnimState.AnimId))
			{
				return SourceSequence->GetPlayLength();
			}
		}
	case EMotionAnimAssetType::BlendSpace:
		{
			if(TObjectPtr<const UMotionBlendSpaceObject> SourceBlendSpace = InMotionData->GetSourceBlendSpaceAtIndex(MMAnimState.AnimId))
			{
				return SourceBlendSpace->GetPlayLength();
			}
		} break;
	case EMotionAnimAssetType::Composite:
		{
			if(TObjectPtr<const UMotionCompositeObject> SourceComposite = InMotionData->GetSourceCompositeAtIndex(MMAnimState.AnimId))
			{
				return SourceComposite->GetPlayLength();
			}
		}
	default: return 0.0f;
	}

	return 0.0f;
}

bool FAnimNode_MSMotionMatching::NextPoseToleranceTest(const FPoseMotionData& NextPose) const
{
	if (NextPose.SearchFlag == EPoseSearchFlag::DoNotUse 
	|| NextPose.MotionTags != RequiredMotionTags
	|| InputData.DesiredInputArray.Num() == 0)
	{
		return false;
	}

	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	const int32 NextPoseStartIndex = NextPose.PoseId * CurrentMotionData->LookupPoseMatrix.AtomCount;

	int32 FeatureOffset = 1; //Start with offset one because we don't use the pose favour for next pose tolerance test
	for(const TObjectPtr<UMatchFeatureBase> Feature : CurrentMotionData->MotionMatchConfig->Features)
	{
		if(Feature->PoseCategory == EPoseCategory::Responsiveness)
		{
			if(!Feature->NextPoseToleranceTest(InputData.DesiredInputArray, CurrentMotionData->LookupPoseMatrix.PoseArray,
				NextPoseStartIndex + FeatureOffset, FeatureOffset, PositionTolerance, RotationTolerance))
			{
				return false;
			}
		}

		FeatureOffset += Feature->Size();
	}

	return true;
}

void FAnimNode_MSMotionMatching::ApplyTrajectoryBlending()
{
	int32 FeatureOffset = 1; //Start at index 1 so as not to apply blending to the pose cost multiplier
	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	for(const TObjectPtr<UMatchFeatureBase> Feature : CurrentMotionData->MotionMatchConfig->Features)
	{
		if(Feature->PoseCategory == EPoseCategory::Responsiveness)
		{
			Feature->ApplyInputBlending(InputData.DesiredInputArray, CurrentInterpolatedPoseArray,
				FeatureOffset, InputResponseBlendMagnitude);
		}

		FeatureOffset += Feature->Size();
	}
}

bool FAnimNode_MSMotionMatching::GenerateCalibrationArray()
{
	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	const int32 CalibrationIndex = CurrentMotionData->GetMotionTagIndex(RequiredMotionTags);

	if(CalibrationIndex < 0
		|| CalibrationIndex >= FinalCalibrationSets.Num())
	{
		return false;
	}
	
	const float OverrideQualityMultiplier = (1.0f - OverrideQualityVsResponsivenessRatio) * 2.0f;
	const float OverrideResponseMultiplier = OverrideQualityVsResponsivenessRatio * 2.0f;
	CalibrationArray = FinalCalibrationSets[CalibrationIndex].Weights;
	
	int32 AtomIndex = 0;
	if(TObjectPtr<const UMotionCalibration> OverrideMotionCalibration = GetUserCalibration())
	{
		if(OverrideMotionCalibration->CalibrationType == EMotionCalibrationType::Multiplier)
		{
			//Apply Multiplier Calibration
			for(TObjectPtr<UMatchFeatureBase> FeaturePtr : CurrentMotionData->MotionMatchConfig->Features)
			{
				const UMatchFeatureBase* Feature = FeaturePtr.Get();
				const int32 FeatureSize = Feature->Size();

				if(Feature->PoseCategory == EPoseCategory::Quality)
				{
					for(int32 i = 0; i < FeatureSize; ++i)
					{
						CalibrationArray[AtomIndex] *= OverrideMotionCalibration->AdjustedCalibrationArray[AtomIndex]
							* OverrideQualityMultiplier;
						
						++AtomIndex;
					}
				}
				else
				{
					for(int32 i = 0; i < FeatureSize; ++i)
					{
						CalibrationArray[AtomIndex] *= OverrideMotionCalibration->AdjustedCalibrationArray[AtomIndex]
							* OverrideResponseMultiplier;
						
						++AtomIndex;
					}
				}
			}
		}
		else
		{
			//Apply Override Calibration
			TArray<float> NormalizerArray = CurrentMotionData->FeatureStandardDeviations[CalibrationIndex].Weights;
			for(TObjectPtr<UMatchFeatureBase> FeaturePtr : CurrentMotionData->MotionMatchConfig->Features)
			{
				const UMatchFeatureBase* Feature = FeaturePtr.Get();
				const int32 FeatureSize = Feature->Size();

				if(Feature->PoseCategory == EPoseCategory::Quality)
				{
					for(int32 i = 0; i < FeatureSize; ++i)
					{
						CalibrationArray[AtomIndex] = NormalizerArray[AtomIndex] *
							OverrideMotionCalibration->AdjustedCalibrationArray[AtomIndex] * OverrideQualityMultiplier;
						++AtomIndex;
					}
				}
				else
				{
					for(int32 i = 0; i < FeatureSize; ++i)
					{
						CalibrationArray[AtomIndex] = NormalizerArray[AtomIndex] *
							OverrideMotionCalibration->AdjustedCalibrationArray[AtomIndex] * OverrideResponseMultiplier;
						++AtomIndex;
					}
				}
			}
		}
	}
	else
	{
		//No Override Motion Calibration used
		for(TObjectPtr<UMatchFeatureBase> FeaturePtr : CurrentMotionData->MotionMatchConfig->Features)
		{
			const UMatchFeatureBase* Feature = FeaturePtr.Get();
			const int32 FeatureSize = Feature->Size();

			if(Feature->PoseCategory == EPoseCategory::Quality)
			{
				for(int32 i = 0; i < FeatureSize; ++i)
				{
					CalibrationArray[AtomIndex] *= OverrideQualityMultiplier;
					++AtomIndex;
				}
			}
			else
			{
				for(int32 i = 0; i < FeatureSize; ++i)
				{
					CalibrationArray[AtomIndex] *= OverrideResponseMultiplier;
					++AtomIndex;
				}
			}
		}
	}
	
	return true;
}

float FAnimNode_MSMotionMatching::GetCurrentAssetTime() const
{
	return InternalTimeAccumulator;
}

float FAnimNode_MSMotionMatching::GetAccumulatedTime() const
{
	return InternalTimeAccumulator;
}


float FAnimNode_MSMotionMatching::GetCurrentAssetTimePlayRateAdjusted() const
{
	if(UAnimSequenceBase* Sequence = GetPrimaryAnim())
	{
		const float EffectivePlayRate = PlaybackRate * (Sequence ? Sequence->RateScale : 1.0f);
		const float Length = Sequence ? Sequence->GetPlayLength() : 0.0f;

		return (EffectivePlayRate < 0.0f) ? Length - InternalTimeAccumulator : InternalTimeAccumulator;
	}

	return 0.0f;
}

float FAnimNode_MSMotionMatching::GetCurrentAssetLength() const
{
	UAnimSequenceBase* Sequence = GetPrimaryAnim();
	return Sequence ? Sequence->GetPlayLength() : 0.0f;
}

UAnimationAsset* FAnimNode_MSMotionMatching::GetAnimAsset() const
{
	return MotionData; //GetMotionData();
}

void FAnimNode_MSMotionMatching::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
	FAnimNode_AssetPlayerBase::Initialize_AnyThread(Context);
	GetEvaluateGraphExposedInputs().Execute(Context);

	InternalTimeAccumulator = 0.0f;

	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	if (!CurrentMotionData 
	|| !CurrentMotionData->bIsProcessed)
	{
		return;
	}
	
	if(!bValidToEvaluate)
	{
		if(CurrentMotionData->MotionMatchConfig->NeedsInitialization())
		{
			CurrentMotionData->MotionMatchConfig->Initialize();
		}
		CheckValidToEvaluate(Context.AnimInstanceProxy);

		if(!bValidToEvaluate)
		{
			return;
		}
	}

	if(bInitialized)
	{
		bTriggerTransition = true;
	}

	AnimInstanceProxy = Context.AnimInstanceProxy;

#if WITH_EDITORONLY_DATA
	//Reset the average pose search debugging data
	AveragePosesChecked = AveragePosesCheckedCounter = 0;
	PosesCheckedMin = INT32_MAX;
	PosesCheckedMax = INT32_MIN;
#endif
}

void FAnimNode_MSMotionMatching::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)
	FAnimNode_AssetPlayerBase::CacheBones_AnyThread(Context);
	const FBoneContainer& BoneContainer = Context.AnimInstanceProxy->GetRequiredBones();
	FillCompactPoseAndComponentRefRotations(BoneContainer);
}

void FAnimNode_MSMotionMatching::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(UpdateAssetPlayer)
	
	GetEvaluateGraphExposedInputs().Execute(Context);

	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	
	if (!CurrentMotionData 
	|| !CurrentMotionData->bIsProcessed
	|| !bValidToEvaluate)
	{
		UE_LOG(LogTemp, Error, TEXT("Motion Matching node failed to update properly as the setup is not valid."))
		return;
	}

	const float DeltaTime = Context.GetDeltaTime();

	if (!bInitialized)
	{
		if(UserCalibration)
		{
			UserCalibration->Initialize();
		}

		
		InitializeWithPoseRecorder(Context);
		bInitialized = true;
	}
	
	UpdateMotionMatchingState(DeltaTime, Context);
	CreateTickRecordForNode(Context, PlaybackRate * MMAnimState.PlayRate);

#if ENABLE_ANIM_DEBUG && ENABLE_DRAW_DEBUG
	//Visualize the motion matching search / optimisation debugging
	const int32 SearchDebugLevel = CVarMMSearchDebug.GetValueOnAnyThread();
	if (SearchDebugLevel == 1)
	{
		DrawSearchCounts(Context.AnimInstanceProxy);
	}

	//Visualize the trajectroy debugging
	const int32 TrajDebugLevel = CVarMMTrajectoryDebug.GetValueOnAnyThread();
	
	if (TrajDebugLevel > 0)
	{
		//Draw Input ArrayFeatures
		DrawInputArrayDebug(Context.AnimInstanceProxy);
	}

	const int32 PoseDebugLevel = CVarMMPoseDebug.GetValueOnAnyThread();

	if (PoseDebugLevel > 0)
	{
		DrawChosenPoseDebug(Context.AnimInstanceProxy, PoseDebugLevel > 1);
	}

	//Debug the current animation data being played by the motion matching node
	const int32 AnimDebugLevel = CVarMMAnimDebug.GetValueOnAnyThread();

	if(AnimDebugLevel > 0)
	{
		DrawAnimDebug(Context.AnimInstanceProxy);
	}
#endif

	//TODO: More Traces and custom trace for Motion matching node

	//TRACE_ANIM_SEQUENCE_PLAYER()
	TRACE_ANIM_NODE_VALUE(Context, TEXT("Name"), CurrentMotionData != nullptr ? CurrentMotionData->GetFName() : NAME_None);
	TRACE_ANIM_NODE_VALUE(Context, TEXT("Motion Data"), CurrentMotionData);
	TRACE_ANIM_NODE_VALUE(Context, TEXT("Current Pose Id"), CurrentChosenPoseId);
}

void FAnimNode_MSMotionMatching::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)
	ANIM_MT_SCOPE_CYCLE_COUNTER_VERBOSE(MSMotionMatching, !IsInGameThread());

	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	
	if (!CurrentMotionData 
	|| !bValidToEvaluate
	|| !CurrentMotionData->bIsProcessed
	|| !IsLODEnabled(Output.AnimInstanceProxy))
	{
		Output.ResetToRefPose();
	}
	else
	{
		EvaluateSinglePose(Output);
	}
	
}

void FAnimNode_MSMotionMatching::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData)
	FString DebugLine = DebugData.GetNodeName(this);

	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	if(CurrentMotionData)
	{
		DebugLine += FString::Printf(TEXT("('%s' Pose Id: %d)"), *CurrentMotionData->GetName(), CurrentInterpolatedPose.PoseId);
		DebugLine += FString::Printf(TEXT("('%s' Anim Id: %d)"), *CurrentMotionData->GetName(), MMAnimState.AnimId);
		DebugLine += FString::Printf(TEXT("('%s' Anim Time: %.3f)"), *CurrentMotionData->GetName(), MMAnimState.AnimTime);
		DebugLine += FString::Printf(TEXT("('%s' Mirrored: %d)"), *CurrentMotionData->GetName(), MMAnimState.bMirrored);
		DebugData.AddDebugItem(DebugLine);
	}
}

void FAnimNode_MSMotionMatching::EvaluateSinglePose(FPoseContext& Output)
{
	float AnimTime = MMAnimState.AnimTime;
	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	
	switch (MMAnimState.AnimType)
	{
		case EMotionAnimAssetType::Sequence:
		{
			if(TObjectPtr<const UMotionSequenceObject> MotionSequence = CurrentMotionData->GetSourceSequenceAtIndex(MMAnimState.AnimId))
			{
				const UAnimSequence* AnimSequence = MotionSequence->Sequence;


				if(MotionSequence->bLoop)
				{
					AnimTime = FMotionMatchingUtils::WrapAnimationTime(AnimTime, AnimSequence->GetPlayLength());
				}
			
				
				FAnimationPoseData AnimationPoseData(Output);
				AnimSequence->GetAnimationPose(AnimationPoseData, FAnimExtractContext(static_cast<double>(AnimTime), true));
			}
		} break;

		case EMotionAnimAssetType::BlendSpace:
		{
			if(TObjectPtr<const UMotionBlendSpaceObject> MotionBlendSpace = CurrentMotionData->GetSourceBlendSpaceAtIndex(MMAnimState.AnimId))
			{
				const UBlendSpace* BlendSpace = MotionBlendSpace->BlendSpace;

				if (!BlendSpace)
				{
					break;
				}

				if (MotionBlendSpace->bLoop)
				{
					AnimTime = FMotionMatchingUtils::WrapAnimationTime(AnimTime, MotionBlendSpace->GetPlayLength());
				}

				for (int32 i = 0; i < MMAnimState.BlendSampleDataCache.Num(); ++i)
				{
					MMAnimState.BlendSampleDataCache[i].Time = AnimTime;
				}
				
				FAnimationPoseData AnimationPoseData(Output);
				BlendSpace->GetAnimationPose(MMAnimState.BlendSampleDataCache, FAnimExtractContext(static_cast<double>(AnimTime),
					Output.AnimInstanceProxy->ShouldExtractRootMotion(), DeltaTimeRecord, MMAnimState.bLoop), AnimationPoseData);
			}
		} break;

		case EMotionAnimAssetType::Composite:
		{
			if(TObjectPtr<const UMotionCompositeObject> MotionComposite = CurrentMotionData->GetSourceCompositeAtIndex(MMAnimState.AnimId))
			{
				const UAnimComposite* Composite = MotionComposite->AnimComposite;

				if(!Composite)
				{
					break;
				}

				if(MotionComposite->bLoop)
				{
					AnimTime = FMotionMatchingUtils::WrapAnimationTime(AnimTime, Composite->GetPlayLength());
				}
				
				FAnimationPoseData AnimationPoseData(Output);
				Composite->GetAnimationPose(AnimationPoseData, FAnimExtractContext(static_cast<double>(MMAnimState.AnimTime), true));
			}
		} break;
		default: ;
	}

	const UMirrorDataTable* MirrorTable = CurrentMotionData->MirrorDataTable;
	if (MMAnimState.bMirrored && MirrorTable)
	{
		const FBoneContainer BoneContainer = Output.Pose.GetBoneContainer();
		const TArray<FBoneIndexType>& RequiredBoneIndices = BoneContainer.GetBoneIndicesArray();
		const int32 NumReqBones = RequiredBoneIndices.Num();
		if(CompactPoseMirrorBones.Num() != NumReqBones)
		{
			FillCompactPoseAndComponentRefRotations(BoneContainer);
		}
		
		FAnimationRuntime::MirrorPose(Output.Pose, MirrorTable->MirrorAxis, CompactPoseMirrorBones, ComponentSpaceRefRotations);
		FAnimationRuntime::MirrorCurves(Output.Curve, *MirrorTable);
		UE::Anim::Attributes::MirrorAttributes(Output.CustomAttributes, *MirrorTable, CompactPoseMirrorBones);
	}
}

void FAnimNode_MSMotionMatching::CreateTickRecordForNode(const FAnimationUpdateContext& Context, const float PlayRate)
{
	// Create a tick record and fill it out
	const float FinalBlendWeight = Context.GetFinalBlendWeight();
	
	UE::Anim::FAnimSyncGroupScope& SyncScope = Context.GetMessageChecked<UE::Anim::FAnimSyncGroupScope>();
	
	const FName GroupNameToUse = ((GetGroupRole() < EAnimGroupRole::TransitionLeader) || bHasBeenFullWeight) ? GetGroupName() : NAME_None;
	EAnimSyncMethod MethodToUse = GetGroupMethod();
	if(GroupNameToUse == NAME_None && MethodToUse == EAnimSyncMethod::SyncGroup)
	{
		MethodToUse = EAnimSyncMethod::DoNotSync;
	}

	const UE::Anim::FAnimSyncParams SyncParams(GroupNameToUse, GetGroupRole(), MethodToUse);
	FAnimTickRecord TickRecord(nullptr, true, PlayRate, false,
		FinalBlendWeight, /*inout*/ InternalTimeAccumulator, MarkerTickRecord);
    
	TickRecord.SourceAsset = MotionData; //GetMotionData();
	TickRecord.TimeAccumulator = &InternalTimeAccumulator;
	TickRecord.MarkerTickRecord = &MarkerTickRecord;
	TickRecord.PlayRateMultiplier = PlayRate;
	TickRecord.EffectiveBlendWeight = FinalBlendWeight;
	TickRecord.bLooping = true;
	TickRecord.bCanUseMarkerSync = false;
	TickRecord.BlendSpace.BlendSpacePositionX = 0.0f;
	TickRecord.BlendSpace.BlendSpacePositionY = 0.0f;
	TickRecord.BlendSpace.BlendFilter = nullptr;
	TickRecord.BlendSpace.BlendSampleDataCache = reinterpret_cast<TArray<FBlendSampleData>*>(&MMAnimState);
	TickRecord.RootMotionWeightModifier = Context.GetRootMotionWeightModifier();
	
	SyncScope.AddTickRecord(TickRecord, SyncParams, UE::Anim::FAnimSyncDebugInfo(Context));
    
	TRACE_ANIM_TICK_RECORD(Context, TickRecord);
}

UAnimSequence* FAnimNode_MSMotionMatching::GetAnimAtIndex(const int32 AnimId)
{
	if(TObjectPtr<const UMotionSequenceObject> Motion = GetMotionData()->GetSourceSequenceAtIndex(MMAnimState.AnimId))
	{
		return Motion->Sequence;
	}
	
	
	return nullptr;
}

UAnimSequenceBase* FAnimNode_MSMotionMatching::GetPrimaryAnim()
{
	if (MMAnimState.AnimId < 0)
	{
		return nullptr;
	}

	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	
	switch (MMAnimState.AnimType)
	{
		case EMotionAnimAssetType::Sequence:
			{
				if(TObjectPtr<const UMotionSequenceObject> Motion = CurrentMotionData->GetSourceSequenceAtIndex(MMAnimState.AnimId))
				{
					return Motion->Sequence;
				}
			} break;
		case EMotionAnimAssetType::Composite:
			{
				if(TObjectPtr<const UMotionCompositeObject> Motion = CurrentMotionData->GetSourceCompositeAtIndex(MMAnimState.AnimId))
				{
					return Motion->AnimComposite;
				}
			} break;
		default: return nullptr;
	}

	return nullptr;
}

UAnimSequenceBase* FAnimNode_MSMotionMatching::GetPrimaryAnim() const
{
	if (MMAnimState.AnimId < 0)
	{
		return nullptr;
	}

	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();

	switch (MMAnimState.AnimType)
	{
		case EMotionAnimAssetType::Sequence:
			{
				if(TObjectPtr<const UMotionSequenceObject> Motion = CurrentMotionData->GetSourceSequenceAtIndex(MMAnimState.AnimId))
				{
					return Motion->Sequence;
				}

			} break;
		case EMotionAnimAssetType::Composite:
			{
				if(TObjectPtr<const UMotionCompositeObject> Motion = CurrentMotionData->GetSourceCompositeAtIndex(MMAnimState.AnimId))
				{
					return Motion->AnimComposite;
				}
			} break;
		default: return nullptr;
	}

	return nullptr;
}

void FAnimNode_MSMotionMatching::DrawInputArrayDebug(FAnimInstanceProxy* InAnimInstanceProxy)
{
#if WITH_EDITOR
	if(!InAnimInstanceProxy
		|| InputData.DesiredInputArray.Num() == 0)
	{
		return;
	}
	
	UMotionMatchConfig* MMConfig = GetMotionData()->MotionMatchConfig;
	
	const FTransform& MeshTransform = InAnimInstanceProxy->GetSkelMeshComponent()->GetComponentTransform();
	const FVector ActorLocation = MeshTransform.GetLocation();
	
	int32 FeatureOffset = 1; //Start at feature offset of 1 for pose cost multiplier
	for(const TObjectPtr<UMatchFeatureBase> Feature : MMConfig->Features)
	{
		if(Feature->PoseCategory == EPoseCategory::Responsiveness)
		{
			Feature->DrawDebugDesiredRuntime(InAnimInstanceProxy, InputData, FeatureOffset, MMConfig);
		}

		FeatureOffset += Feature->Size();
	}
#endif
}

void FAnimNode_MSMotionMatching::DrawChosenInputArrayDebug(FAnimInstanceProxy* InAnimInstanceProxy)
{
#if WITH_EDITOR
	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	
	if (InAnimInstanceProxy == nullptr 
	|| CurrentChosenPoseId > CurrentMotionData->Poses.Num() - 1)
	{
		return;
	}

	UMotionMatchConfig* MMConfig = CurrentMotionData->MotionMatchConfig;

	int32 FeatureOffset = 1; //Start at feature offset of 1 for pose cost multiplier
	for(const TObjectPtr<UMatchFeatureBase> Feature : MMConfig->InputResponseFeatures)
	{
		Feature->DrawDebugCurrentRuntime(InAnimInstanceProxy, CurrentMotionData, CurrentInterpolatedPoseArray, FeatureOffset);
		FeatureOffset += Feature->Size();
	}
#endif
}

void FAnimNode_MSMotionMatching::DrawChosenPoseDebug(FAnimInstanceProxy* InAnimInstanceProxy, bool bDrawVelocity)
{
#if WITH_EDITOR
	TObjectPtr<const UMotionDataAsset> CurrentMotionData = GetMotionData();
	
	if (InAnimInstanceProxy == nullptr 
	|| CurrentChosenPoseId > CurrentMotionData->Poses.Num() - 1)
	{
		return;
	}

	UMotionMatchConfig* MMConfig = CurrentMotionData->MotionMatchConfig;

	int32 FeatureOffset = 1; //Start at feature offset of 1 for pose cost multiplier
	for(const TObjectPtr<UMatchFeatureBase> Feature : MMConfig->Features)
	{
		if(Feature->PoseCategory == EPoseCategory::Quality)
		{
			Feature->DrawDebugCurrentRuntime(InAnimInstanceProxy,CurrentMotionData, CurrentInterpolatedPoseArray, FeatureOffset);
		}

		FeatureOffset += Feature->Size();
	}
#endif
}

void FAnimNode_MSMotionMatching::DrawSearchCounts(FAnimInstanceProxy* InAnimInstanceProxy)
{
#if WITH_EDITORONLY_DATA
	if (!InAnimInstanceProxy)
	{
		return;
	}
	const int32 PoseCount = GetMotionData()->Poses.Num();

	const FString TotalMessage = FString::Printf(TEXT("Total Poses: %01d"), PoseCount);
	const FString LastMessage = FString::Printf(TEXT("Poses Searched: %01d (%02f Reduction)"), PosesChecked,
		(static_cast<float>(PoseCount) - static_cast<float>(PosesChecked)) / static_cast<float>(PoseCount) * 100.0f);
	const FString AverageMessage = FString::Printf(TEXT("Average: %01d, Min: %02d, Max: %01d (%02f reduction)"),
		AveragePosesChecked, PosesCheckedMin, PosesCheckedMax,
		(static_cast<float>(PoseCount) - static_cast<float>(AveragePosesChecked)) / static_cast<float>(PoseCount) * 100.0f);
	const FString InnerAABBMessage = FString::Printf(TEXT(
		"Out of %02d InnerAABBs tested, %01d passed and were searched"), InnerAABBsChecked, InnerAABBsPassed);
	const FString OuterAABBMessage = FString::Printf(TEXT(
		"Out of %02d OuterAABBs tested, %01d passed and were searched"), OuterAABBsChecked, OuterAABBsPassed);
	
	InAnimInstanceProxy->AnimDrawDebugOnScreenMessage(TotalMessage, FColor::Black);
	InAnimInstanceProxy->AnimDrawDebugOnScreenMessage(LastMessage, FColor::Purple);
	InAnimInstanceProxy->AnimDrawDebugOnScreenMessage(AverageMessage, FColor::Orange);
	InAnimInstanceProxy->AnimDrawDebugOnScreenMessage(InnerAABBMessage, FColor::Blue);
	InAnimInstanceProxy->AnimDrawDebugOnScreenMessage(OuterAABBMessage, FColor::Red);
#endif
}

void FAnimNode_MSMotionMatching::DrawAnimDebug(FAnimInstanceProxy* InAnimInstanceProxy) const
{
	if(!InAnimInstanceProxy)
	{
		return;
	}

	TObjectPtr<UMotionDataAsset> CurrentMotionData = MotionData;//GetMotionData();

	const FPoseMotionData& CurrentPose = CurrentMotionData->Poses[FMath::Clamp(CurrentInterpolatedPose.PoseId,
	0, CurrentMotionData->Poses.Num() - 1)];
	
	//Print Pose Information
	FString Message = FString::Printf(TEXT("Pose Id: %02d \nPoseFavour: %f \nMirrored: "),
		CurrentPose.PoseId, CurrentMotionData->GetPoseFavour(CurrentPose.PoseId));
	
	if(CurrentPose.bMirrored)
	{
		Message += FString(TEXT("True\n"));
	}
	else
	{
		Message += FString(TEXT("False\n"));
	}
	
	InAnimInstanceProxy->AnimDrawDebugOnScreenMessage(Message, FColor::Green);
	
	//Print Animation Information
	FString AnimMessage = FString::Printf(TEXT("Anim Id: %02d \nAnimType: "), CurrentPose.AnimId);
	

	switch(CurrentPose.AnimType)
	{
		case EMotionAnimAssetType::Sequence: AnimMessage += FString(TEXT("Sequence \n")); break;
		case EMotionAnimAssetType::BlendSpace: AnimMessage += FString(TEXT("Blend Space \n")); break;
		case EMotionAnimAssetType::Composite: AnimMessage += FString(TEXT("Composite \n")); break;
		default: AnimMessage += FString(TEXT("Invalid \n")); break;
	}
	AnimMessage += FString::Printf(TEXT("Anim Time: %0f \nAnimName: "), MMAnimState.AnimTime);

	TObjectPtr<const UMotionAnimObject> MotionAnimAsset = CurrentMotionData->GetEditableSourceAnim(CurrentPose.AnimId, CurrentPose.AnimType);
	if(MotionAnimAsset && MotionAnimAsset->AnimAsset)
	{
		AnimMessage += MotionAnimAsset->AnimAsset->GetName();
	}
	else
	{
		AnimMessage += FString::Printf(TEXT("Invalid \n"));
	}

	
	InAnimInstanceProxy->AnimDrawDebugOnScreenMessage(AnimMessage, FColor::Blue);

	//Tags
	FString TagMessage = FString::Printf(TEXT("\nTags: ")) + RequiredMotionTags.ToString();
	InAnimInstanceProxy->AnimDrawDebugOnScreenMessage(TagMessage, FColor::Orange);


}

void FAnimNode_MSMotionMatching::RecordHistoricalPoseSearch(const int32 InPosesSearched)
{
#if WITH_EDITORONLY_DATA
	if(InPosesSearched == 0)
	{
		return;
	}
	
	if(InPosesSearched < PosesCheckedMin)
	{
		PosesCheckedMin = InPosesSearched;
	}
	else if(InPosesSearched > PosesCheckedMax)
	{
		PosesCheckedMax = InPosesSearched;
	}
	
	
	const int32 Cumulative = AveragePosesChecked * AveragePosesCheckedCounter + InPosesSearched;
	++AveragePosesCheckedCounter;
	
	AveragePosesChecked = Cumulative / AveragePosesCheckedCounter;
	
	if(Cumulative > INT32_MAX / 2)
	{
		AveragePosesCheckedCounter = 50;
	}
#endif
}

void FAnimNode_MSMotionMatching::FillCompactPoseAndComponentRefRotations(const FBoneContainer& BoneContainer)
{
	if(const UMirrorDataTable* MirrorDataTable = GetMirrorDataTable())
	{
		MirrorDataTable->FillCompactPoseAndComponentRefRotations(
			BoneContainer,
			CompactPoseMirrorBones,
			ComponentSpaceRefRotations);
	}
	else
	{
		CompactPoseMirrorBones.Reset();
		ComponentSpaceRefRotations.Reset();
	}
}
