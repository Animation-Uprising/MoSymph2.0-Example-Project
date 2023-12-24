//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimGraph/AnimNode_PoseMatchBase.h"
#include "AnimNode_MotionRecorder.h"
#include "DrawDebugHelpers.h"
#include "MMPreProcessUtils.h"
#include "MotionAnimAsset.h"
#include "MotionMatchConfig.h"
#include "Animation/MirrorDataTable.h"
#include "MatchFeatures/MatchFeatureBase.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Utility/MotionMatchingUtils.h"

#define LOCTEXT_NAMESPACE "MotionSymphonyNodes"

static TAutoConsoleVariable<int32> CVarPoseMatchingDebug(
	TEXT("a.AnimNode.MoSymph.PoseMatch.Debug"),
	0,
	TEXT("Turns Pose Matching Debugging On / Off.\n")
	TEXT("<=0: Off \n")
	TEXT("  1: On - Show chosen pose\n")
	TEXT("  2: On - Show All Poses\n")
	TEXT("  3: On - Show All Poses With Velocity\n"));


FPoseMatchData::FPoseMatchData()
	: PoseId(-1),
	AnimId(0),
	bMirror(false),
	Time(0.0f)
{
}

FPoseMatchData::FPoseMatchData(int32 InPoseId, int32 InAnimId, float InTime, bool bInMirror)
	: PoseId(InPoseId),
	AnimId(InAnimId),
	bMirror(bInMirror),
	Time(InTime)
{
}

FAnimNode_PoseMatchBase::FAnimNode_PoseMatchBase()
	: PoseInterval(0.1f),
	  PosesEndTime(5.0f),
	  bEnableMirroring(false),
	  bInitialized(false),
	  bInitPoseSearch(false),
	  PoseRecorderConfigIndex(0),
	  MatchPose(nullptr),
	  MatchPoseIndex(0),
	  AnimInstanceProxy(nullptr),
	  bIsDirtyForPreProcess(true)
{
}

void FAnimNode_PoseMatchBase::PreProcess()
{
	Poses.Empty();
	bIsDirtyForPreProcess = false;
}

void FAnimNode_PoseMatchBase::SetDirtyForPreProcess()
{
	bIsDirtyForPreProcess = true;
}

void FAnimNode_PoseMatchBase::PreProcessAnimation(UAnimSequence* Anim, int32 AnimIndex, bool bMirror/* = false*/)
{
	if(!Anim 
	|| !PoseConfig)
	{
		return;
	}

	if (bMirror 
	&& !MirrorDataTable) //Cannot pre-process mirrored animation if mirror data table is null
	{
		return;
	}

	const float AnimLength = FMath::Min(Anim->GetPlayLength(), PosesEndTime);
	
	
	if(PoseInterval < 0.01f)
	{
		PoseInterval = 0.01f;
	}
	
	float TotalMatrixSize = PoseConfig->TotalDimensionCount * (FMath::FloorToInt32(AnimLength / PoseInterval) + 1);
	if(bMirror && MirrorDataTable)
	{
		TotalMatrixSize *= 2;
	}
	PoseMatrix.SetNumZeroed(TotalMatrixSize);

	//Non Mirror Pass
	PreProcessAnimPass(Anim, AnimLength, AnimIndex, false);
	
	if(bMirror)
	{
		PreProcessAnimPass(Anim, AnimLength, AnimIndex, true);
	}
}

void FAnimNode_PoseMatchBase::InitializeData()
{
	if(!PoseConfig)
	{
		return;
	}

	if(PoseConfig->NeedsInitialization())
	{
		PoseConfig->Initialize();
	}
	
	if(bIsDirtyForPreProcess)
	{
		PreProcess();
	}

	//Generate the default weightings for calibration
	if(Calibration)
	{
		Calibration->OnGenerateWeightings();
	}
	
	//Calculate feature normalization calibrations
	StandardDeviation.Initialize(PoseConfig->TotalDimensionCount);
	StandardDeviation.GenerateStandardDeviationWeights(PoseMatrix, PoseConfig);

	//Generate Final Weights
	FinalCalibration.Initialize(PoseConfig);
	FinalCalibration.GenerateFinalWeights(PoseConfig, StandardDeviation);
}

void FAnimNode_PoseMatchBase::FindMatchPose(const FAnimationUpdateContext& Context)
{
	if(Poses.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("FAnimNode_PoseMatchBase: No poses recorded in node"))
		return;
	}
	
	FAnimNode_MotionRecorder* MotionRecorderNode = nullptr;
	if (IMotionSnapper* MotionSnapper = Context.GetMessage<IMotionSnapper>())
	{
		MotionRecorderNode = &MotionSnapper->GetNode();
	}

	if (MotionRecorderNode)
	{
		if (!bInitialized)
		{
			PoseRecorderConfigIndex = MotionRecorderNode->RegisterMotionMatchConfig(PoseConfig);
			bInitialized = true;
		}

		const TArray<float>* CurrentPoseArray = MotionRecorderNode->GetCurrentPoseArray(PoseRecorderConfigIndex);
		const int32 MinimaCostPoseId = FMath::Clamp(GetMinimaCostPoseId(CurrentPoseArray), 0, Poses.Num() - 1);

		MatchPose = &Poses[MinimaCostPoseId];
		MatchPoseIndex = MinimaCostPoseId;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FAnimNode_PoseMatchBase: Cannot find Motion Snapshot node to pose match against."))
		MatchPose = &Poses[0];
		MatchPoseIndex = 0;
	}

	SetSequence(FindActiveAnim());
	InternalTimeAccumulator = MatchPose->Time;
	SetStartPosition(InternalTimeAccumulator);
	PlayRateScaleBiasClampState.Reinitialize();
}

UAnimSequenceBase* FAnimNode_PoseMatchBase::FindActiveAnim()
{
	return nullptr;
}

int32 FAnimNode_PoseMatchBase::GetMinimaCostPoseId(const TArray<float>* InCurrentPoseArray)
{
	if (!InCurrentPoseArray
		|| Poses.Num() == 0
		|| FinalCalibration.Weights.Num() == 0)
	{
		return -1;
	}
	
	int32 MinimaCostPoseId = 0;
	float MinimaCost = 10000000.0f;

	const int32 AtomCount = PoseConfig->TotalDimensionCount;
	for(int32 PoseIndex = 0; PoseIndex < Poses.Num(); ++PoseIndex)
	{
		float Cost = 0.0f;
		const int32 MatrixStartIndex = PoseIndex * AtomCount;
		for(int32 AtomIndex = 0; AtomIndex < AtomCount; ++AtomIndex)
		{
			Cost += FMath::Abs(PoseMatrix[MatrixStartIndex + AtomIndex] - (*InCurrentPoseArray)[AtomIndex])
				* FinalCalibration.Weights[AtomIndex];
		}

		if(Cost < MinimaCost)
		{
			MinimaCost = Cost;
			MinimaCostPoseId = PoseIndex;
		}
	}

	return MinimaCostPoseId;
}

int32 FAnimNode_PoseMatchBase::GetMinimaCostPoseId(const TArray<float>& InCurrentPoseArray, float& OutCost,
                                                   int32 InStartPoseId, int32 InEndPoseId)
{
	if (Poses.Num() == 0)
	{
		return -1;
	}

	InStartPoseId = FMath::Clamp(InStartPoseId, 0, Poses.Num() - 1);
	InEndPoseId = FMath::Clamp(InEndPoseId, 0, Poses.Num() - 1);

	int32 MinimaCostPoseId = 0;
	OutCost = 10000000.0f;
	const int32 AtomCount = PoseConfig->TotalDimensionCount;
	for(int32 PoseIndex = InStartPoseId; PoseIndex < InEndPoseId; ++PoseIndex)
	{
		float Cost = 0.0f;
		const int32 MatrixStartIndex = PoseIndex * AtomCount;
		for(int32 AtomIndex = 0; AtomIndex < AtomCount; ++AtomIndex)
		{
			Cost += FMath::Abs(PoseMatrix[MatrixStartIndex + AtomIndex] - InCurrentPoseArray[AtomIndex])
				* FinalCalibration.Weights[AtomIndex];
		}

		if(Cost < OutCost)
		{
			OutCost = Cost;
			MinimaCostPoseId = PoseIndex;
		}
	}

	return MinimaCostPoseId;
}

float FAnimNode_PoseMatchBase::ComputeSinglePoseCost(const TArray<float>& InCurrentPoseArray, const int32 InPoseIndex)
{
	float Cost = 0.0f;
	const int32 MatrixStartIndex = InPoseIndex * PoseConfig->TotalDimensionCount;
	for(int32 AtomIndex = 0; AtomIndex < PoseConfig->TotalDimensionCount; ++AtomIndex)
	{
		Cost += FMath::Abs(PoseMatrix[MatrixStartIndex + AtomIndex] - InCurrentPoseArray[AtomIndex])
			* FinalCalibration.Weights[AtomIndex];;
	}

	return Cost;
}

void FAnimNode_PoseMatchBase::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_AssetPlayerBase::Initialize_AnyThread(Context);
	GetEvaluateGraphExposedInputs().Execute(Context);

	bInitPoseSearch = true;
	AnimInstanceProxy = Context.AnimInstanceProxy;

	if(!bInitialized)
	{
		InitializeData();
	}
}

void FAnimNode_PoseMatchBase::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread);
	FAnimNode_SequencePlayer::CacheBones_AnyThread(Context);
	
	const FBoneContainer& BoneContainer = Context.AnimInstanceProxy->GetRequiredBones();
	FillCompactPoseAndComponentRefRotations(BoneContainer);
}

void FAnimNode_PoseMatchBase::UpdateAssetPlayer(const FAnimationUpdateContext & Context)
{
	GetEvaluateGraphExposedInputs().Execute(Context);

	UAnimSequenceBase* CacheSequence = GetSequence();
	const float CachePlayRateBasis = GetPlayRateBasis();

	if (bInitPoseSearch)
	{
		FindMatchPose(Context); //Override this to setup the animation data
		
		if (MatchPose && CacheSequence)
		{
			SetStartPosition(FMath::Clamp(GetStartPosition(), 0.0f, CacheSequence->GetPlayLength()));
			InternalTimeAccumulator = GetStartPosition();
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
	else
	{
		if(CacheSequence)
		{
			InternalTimeAccumulator = FMath::Clamp(InternalTimeAccumulator, 0.f, CacheSequence->GetPlayLength());
			const float AdjustedPlayRate = PlayRateScaleBiasClampState.ApplyTo(GetPlayRateScaleBiasClampConstants(),
				FMath::IsNearlyZero(CachePlayRateBasis) ? 0.0f : (GetPlayRate() / CachePlayRateBasis), 0.0f);
#if ENGINE_MINOR_VERSION > 2
			CreateTickRecordForNode(Context, CacheSequence, IsLooping(), AdjustedPlayRate, false);
#else
			CreateTickRecordForNode(Context, CacheSequence, GetLoopAnimation(), AdjustedPlayRate, false);
#endif
		}
	}

#if WITH_EDITORONLY_DATA
	if (FAnimBlueprintDebugData* DebugData = Context.AnimInstanceProxy->GetAnimBlueprintDebugData())
	{
		const int32 NumFrames = Sequence->GetNumberOfSampledKeys();
		DebugData->RecordSequencePlayer(Context.GetCurrentNodeId(), GetAccumulatedTime(), Sequence != nullptr ? Sequence->GetPlayLength() : 0.0f, Sequence != nullptr ? NumFrames : 0);
	}
#endif

#if ENABLE_ANIM_DEBUG && ENABLE_DRAW_DEBUG
	if (AnimInstanceProxy && MatchPose)
	{
		const USkeletalMeshComponent* SkelMeshComp = AnimInstanceProxy->GetSkelMeshComponent();
		const int32 DebugLevel = CVarPoseMatchingDebug.GetValueOnAnyThread();

		if (DebugLevel > 0)
		{
			DrawPoseMatchDebug(Context.AnimInstanceProxy);
		}
	}
#endif
	
	TRACE_ANIM_SEQUENCE_PLAYER(Context, *this);
	TRACE_ANIM_NODE_VALUE(Context, TEXT("Name"), CacheSequence != nullptr ? CacheSequence->GetFName() : NAME_None);
	TRACE_ANIM_NODE_VALUE(Context, TEXT("Sequence"), CacheSequence);
	TRACE_ANIM_NODE_VALUE(Context, TEXT("Playback Time"), InternalTimeAccumulator);
}

void FAnimNode_PoseMatchBase::Evaluate_AnyThread(FPoseContext& Output)
{
	Super::Evaluate_AnyThread(Output);

	if (MatchPose 
	    && MatchPose->bMirror 
		&& MirrorDataTable
		&& IsLODEnabled(Output.AnimInstanceProxy))
	{
		const FBoneContainer BoneContainer = Output.Pose.GetBoneContainer();
		const TArray<FBoneIndexType>& RequiredBoneIndices = BoneContainer.GetBoneIndicesArray();
		const int32 NumReqBones = RequiredBoneIndices.Num();
		if(CompactPoseMirrorBones.Num() != NumReqBones)
		{
			FillCompactPoseAndComponentRefRotations(BoneContainer);
		}
	
		FAnimationRuntime::MirrorPose(Output.Pose, MirrorDataTable->MirrorAxis, CompactPoseMirrorBones, ComponentSpaceRefRotations);
		FAnimationRuntime::MirrorCurves(Output.Curve, *MirrorDataTable);
		UE::Anim::Attributes::MirrorAttributes(Output.CustomAttributes, *MirrorDataTable, CompactPoseMirrorBones);
	}
}

USkeleton* FAnimNode_PoseMatchBase::GetNodeSkeleton()
{
	const UAnimSequenceBase* CacheSequence = GetSequence();
	
	return CacheSequence? CacheSequence->GetSkeleton() : nullptr;
}

void FAnimNode_PoseMatchBase::PreProcessAnimPass(UAnimSequence* Anim, const float AnimLength, const int32 AnimIndex, const bool bMirror)
{
	float CurrentTime = 0.0f;
	while (CurrentTime <= AnimLength)
	{
		const int32 PoseId = Poses.Num(); //Todo: Float error accuracy can cause this PoseId to crash something
		FPoseMatchData NewPoseData = FPoseMatchData(PoseId, AnimIndex, CurrentTime, bMirror);

		int32 CurrentFeatureOffset = 0;
		for(TObjectPtr<UMatchFeatureBase> MatchFeature : PoseConfig->Features)
		{
			if(MatchFeature)
			{
				float* ResultLocation = &PoseMatrix[PoseId * PoseConfig->TotalDimensionCount + CurrentFeatureOffset];
				MatchFeature->EvaluatePreProcess(ResultLocation, Anim, CurrentTime, PoseInterval, bMirror, MirrorDataTable, nullptr);
				
				CurrentFeatureOffset += MatchFeature->Size();
			}
		}
		
		Poses.Add(NewPoseData);
		CurrentTime += PoseInterval;
	}
}

void FAnimNode_PoseMatchBase::FillCompactPoseAndComponentRefRotations(const FBoneContainer& BoneContainer)
{
	if(MirrorDataTable)
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

void FAnimNode_PoseMatchBase::DrawPoseMatchDebug(const FAnimInstanceProxy* InAnimInstanceProxy)
{
	if(!InAnimInstanceProxy)
	{
		return;
	}
	
	// const FTransform ComponentTransform = AnimInstanceProxy->GetComponentTransform();
	//
	// for (FJointData& JointData : MatchPose->BoneData)
	// {
	// 	FVector Point = ComponentTransform.TransformPosition(JointData.Position);
	//
	// 	AnimInstanceProxy->AnimDrawDebugSphere(Point, 10.0f, 12.0f, FColor::Yellow, false, -1.0f, 0.5f);
	// }
	//
	// if(DebugLevel > 1)
	// {
	// 	for (int i = 0; i < PoseConfig.Num(); ++i)
	// 	{
	// 		const float Progress = ((float)i) / ((float)PoseConfig.Num() - 1);
	// 		FColor Color = (FLinearColor::Blue + Progress * (FLinearColor::Red - FLinearColor::Blue)).ToFColor(true);
	//
	// 		FVector LastPoint = FVector::ZeroVector;
	// 		int LastAnimId = -1;
	// 		for (FPoseMatchData& Pose : Poses)
	// 		{
	// 			FVector Point = ComponentTransform.TransformPosition(Pose.BoneData[i].Position);
	// 					
	// 			AnimInstanceProxy->AnimDrawDebugSphere(Point, 3.0f, 6.0f, Color, false, -1.0f, 0.25f);
	//
	// 			if(DebugLevel > 2)
	// 			{
	// 				FVector ArrowPoint = ComponentTransform.TransformVector(Pose.BoneData[i].Velocity) * 0.33333f;
	// 				AnimInstanceProxy->AnimDrawDebugDirectionalArrow(Point, ArrowPoint, 20.0f, Color, false, -1.0f, 0.0f);
	// 			}
	// 					
	// 			if(Pose.AnimId == LastAnimId)
	// 			{
	// 				AnimInstanceProxy->AnimDrawDebugLine(LastPoint, Point, Color, false, -1.0f, 0.0f);
	// 			}
	//
	// 			LastAnimId = Pose.AnimId;
	// 			LastPoint = Point;
	// 		}
	//
	// 	}
	// }
}

#undef LOCTEXT_NAMESPACE
