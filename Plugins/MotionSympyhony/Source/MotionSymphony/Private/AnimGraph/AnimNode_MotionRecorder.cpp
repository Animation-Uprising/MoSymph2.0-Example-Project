//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimGraph/AnimNode_MotionRecorder.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimInstanceProxy.h"
#include "DrawDebugHelpers.h"
#include "MotionMatchConfig.h"

#define LOCTEXT_NAMESPACE "AnimNode_PoseRecorder"

static TAutoConsoleVariable<int32> CVarMotionSnapshotDebug(
	TEXT("a.AnimNode.MoSymph.MotionSnapshot.Debug"),
	0,
	TEXT("Turns Motion Recorder Debugging On / Off.\n")
	TEXT("<=0: Off \n")
	TEXT("  1: On - Minimal\n")
	TEXT("  2: On - Show Velocity\n"));

static TAutoConsoleVariable<int32> CVarMotionSnapshotEnable(
	TEXT("a.AnimNode.MoSymph.MotionSnapshot.Enable"),
	1,
	TEXT("Turns Motion Recorder Node On / Off.\n")
	TEXT("<=0: Off \n")
	TEXT("  1: On\n"));

IMPLEMENT_ANIMGRAPH_MESSAGE(IMotionSnapper);
const FName IMotionSnapper::Attribute("MotionSnapshot");

class FMotionSnapper : public IMotionSnapper
{
private:
	//Node to target
	FAnimNode_MotionRecorder& Node;

	//Node index
	int32 NodeId;

	//Proxy currently executing
	FAnimInstanceProxy& AnimInstanceProxy;

public:
	FMotionSnapper(const FAnimationBaseContext& InContext, FAnimNode_MotionRecorder* InNode)
		: Node(*InNode),
		NodeId(InContext.GetCurrentNodeId()),
		AnimInstanceProxy(*InContext.AnimInstanceProxy)
	{}

private:
	//IMotionSnapper interface
	virtual struct FAnimNode_MotionRecorder& GetNode() override
	{
		return Node;
	}
	

	virtual void AddDebugRecord(const FAnimInstanceProxy& InSourceProxy, int32 InSourceNodeId)
	{
#if WITH_EDITORONLY_DATA
		AnimInstanceProxy.RecordNodeAttribute(InSourceProxy, NodeId, InSourceNodeId, IMotionSnapper::Attribute);
#endif
		TRACE_ANIM_NODE_ATTRIBUTE(AnimInstanceProxy, InSourceProxy, NodeId, InSourceNodeId, IMotionSnapper::Attribute);
	}
};


FMotionRecordData::FMotionRecordData()
{
}

FMotionRecordData::FMotionRecordData(UMotionMatchConfig* InMotionMatchConfig)
{
	if(!InMotionMatchConfig)
	{
		//Todo: UE_LOG
		return;
	}

	MotionMatchConfig = InMotionMatchConfig;
	
	RecordedPoseArray.SetNumZeroed(InMotionMatchConfig->TotalDimensionCount);
	FeatureCacheData.SetNumZeroed(InMotionMatchConfig->TotalDimensionCount);
}

FAnimNode_MotionRecorder::FAnimNode_MotionRecorder()
	: bRetargetPose(true),
      PoseDeltaTime(0),
	  AnimInstanceProxy(nullptr)
{
	MotionConfigs.Empty(3);
	MotionRecorderData.Empty(3);
}

const TArray<float>* FAnimNode_MotionRecorder::GetCurrentPoseArray(const UMotionMatchConfig* InConfig)
{
	if(!InConfig)
	{
		return nullptr;
	}
	
	const int32 ConfigIndex = GetMotionConfigIndex(InConfig);
	if(ConfigIndex > -1 || ConfigIndex < MotionRecorderData.Num())
	{
		return &MotionRecorderData[ConfigIndex].RecordedPoseArray;
	}

	return nullptr;
}

const TArray<float>* FAnimNode_MotionRecorder::GetCurrentPoseArray(const int32 ConfigIndex)
{
	if(ConfigIndex > -1 && ConfigIndex < MotionRecorderData.Num())
	{
		return &MotionRecorderData[ConfigIndex].RecordedPoseArray;
	}
	
	return nullptr;
}

int32 FAnimNode_MotionRecorder::GetMotionConfigIndex(const UMotionMatchConfig* InConfig)
{
	const int32 ConfigIterations = FMath::Min(MotionConfigs.Num(), MotionRecorderData.Num());
	for(int32 ConfigIndex = 0; ConfigIndex < ConfigIterations; ++ConfigIndex)
	{
		if(InConfig == MotionConfigs[ConfigIndex])
		{
			return ConfigIndex;
		}
	}

	return -1;
}

int32 FAnimNode_MotionRecorder::RegisterMotionMatchConfig(UMotionMatchConfig* InMotionMatchConfig)
{
	if(!InMotionMatchConfig)
	{
		return -1;
	}

	int32 FoundIndex = -1;
	if(MotionConfigs.Find(InMotionMatchConfig, FoundIndex))
	{
		return FoundIndex;
	}

	if(InMotionMatchConfig->NeedsInitialization())
	{
		InMotionMatchConfig->Initialize();
	}

	MotionConfigs.Add(InMotionMatchConfig);
	MotionRecorderData.Add(FMotionRecordData(InMotionMatchConfig));

	CacheMotionBones(AnimInstanceProxy);

	return MotionConfigs.Num() - 1;
}

void FAnimNode_MotionRecorder::LogRequestError(const FAnimationUpdateContext& Context, const FPoseLinkBase& RequesterPoseLink)
{
#if WITH_EDITORONLY_DATA
	UAnimBlueprint* AnimBlueprint = Context.AnimInstanceProxy->GetAnimBlueprint();
	UAnimBlueprintGeneratedClass* AnimClass = AnimBlueprint ? AnimBlueprint->GetAnimBlueprintGeneratedClass() : nullptr;
	const UObject* RequesterNode = AnimClass ? AnimClass->GetVisualNodeFromNodePropertyIndex(RequesterPoseLink.SourceLinkID) : nullptr;

	FText Message = FText::Format(LOCTEXT("MotionSnapperRequestError", "No Motion Snapper node found for request from '{0}'. Add a motionsnapper node after this request."),
		FText::FromString(GetPathNameSafe(RequesterNode)));
#endif
}

void FAnimNode_MotionRecorder::OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy,
	const UAnimInstance* InAnimInstance)
{
	for(int32 ConfigIndex = 0; ConfigIndex < MotionConfigs.Num(); ++ConfigIndex)
	{
		if(TObjectPtr<UMotionMatchConfig> Config = MotionConfigs[ConfigIndex])
		{
			Config->Initialize();
			MotionRecorderData.Add(FMotionRecordData(Config));
		}
		else
		{
			MotionConfigs.RemoveAt(ConfigIndex);
			--ConfigIndex;
		}
	}
	CacheMotionBones(InProxy);
}

void FAnimNode_MotionRecorder::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread);

	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);	

	AnimInstanceProxy = Context.AnimInstanceProxy;
}

void FAnimNode_MotionRecorder::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread);

	FAnimNode_Base::CacheBones_AnyThread(Context);
	Source.CacheBones(Context);
	
	CacheMotionBones(Context.AnimInstanceProxy);
}

void FAnimNode_MotionRecorder::CacheMotionBones(const FAnimInstanceProxy* InProxy)
{
	if (!InProxy)
	{
		return;
	}
	
	const FBoneContainer& BoneContainer = InProxy->GetRequiredBones();

	const int32 ConfigIterations = FMath::Min(MotionConfigs.Num(), MotionRecorderData.Num());
	for(int32 ConfigIndex = 0; ConfigIndex < ConfigIterations; ++ConfigIndex)
	{
		if(UMotionMatchConfig* MotionConfig = MotionConfigs[ConfigIndex])
		{
			for(TObjectPtr<UMatchFeatureBase> Feature : MotionConfig->Features)
			{
				if(Feature)
				{
					Feature->CacheMotionBones(InProxy);
				}
			}
		}
	}
}

void FAnimNode_MotionRecorder::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread);
	
	//Allow nodes further towards the leaves to use the motion snapshot node
	UE::Anim::TScopedGraphMessage<FMotionSnapper> MotionSnapper(Context, Context, this);

	Source.Update(Context);

	PoseDeltaTime = Context.AnimInstanceProxy->GetDeltaSeconds();
}

void FAnimNode_MotionRecorder::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread);

	Source.Evaluate(Output);

	FComponentSpacePoseContext CS_Output(Output.AnimInstanceProxy);

	if (bRetargetPose)
	{
		//Create a new retargeted pose, initialize it from our current pose
		FCompactPose RetargetedPose(Output.Pose);

		//Pull the bones out so we can use them directly
		TArray<FTransform> RetargetedToBase;
		RetargetedPose.CopyBonesTo(RetargetedToBase); //The actual current Pose which is additive to the reference pose of the current skeleton 
		const TArray<FTransform>& CompactRefPose = RetargetedPose.GetBoneContainer().GetRefPoseArray();
		
		const TArray<FTransform>& RefSkeletonRefPose = Output.AnimInstanceProxy->GetSkeleton()->GetReferenceSkeleton().GetRefBonePose();	//The reference pose of the reference skeleton

		for (int32 i = 0; i < FMath::Min(CompactRefPose.Num(), RetargetedToBase.Num()); ++i)
		{
			FTransform& RetargetBoneTransform = RetargetedToBase[i];											//The actual current bone transform					
			//const FTransform& ModelBoneTransform = CompactRefPose[i];												//The bone transform of the reference pose (current skeleton)
			
			const int32 SkeletonPoseIndex = Output.Pose.GetBoneContainer().GetSkeletonPoseIndexFromMeshPoseIndex(FMeshPoseBoneIndex(i)).GetInt(); //Todo:: Test if this is even correct
			//const FTransform& RefSkeletonBoneTransform = RefSkeletonRefPose[SkeletonPoseIndex];

			//FTransform BoneTransform;

			//(ActualBone / RefPoseBone) * RefSkelRefPoseBone			[For Rotation & Scale]
			//(ActualBone - RePoseBone) + RefSkelRefPoseBone			[For Translation]

			/*RetargetBoneTransform.SetRotation(RefSkeletonBoneTransform.GetRotation() * ModelBoneTransform.GetRotation().Inverse() *  RetargetBoneTransform.GetRotation());
			RetargetBoneTransform.SetTranslation(RetargetBoneTransform.GetTranslation() - ModelBoneTransform.GetTranslation() + RefSkeletonBoneTransform.GetTranslation());
			RetargetBoneTransform.SetScale3D(RetargetBoneTransform.GetScale3D() * ModelBoneTransform.GetSafeScaleReciprocal(ModelBoneTransform.GetScale3D() * (RefSkeletonBoneTransform.GetScale3D())));*/

			RetargetedToBase[i] = (RetargetedToBase[i] * CompactRefPose[i].Inverse()) * RefSkeletonRefPose[SkeletonPoseIndex];
			RetargetBoneTransform.NormalizeRotation();
		}

		//Set the bones back
		RetargetedPose.CopyBonesFrom(RetargetedToBase);

		//Convert pose to component space
		CS_Output.Pose.InitPose(RetargetedPose);
	}
	else
	{
		//Convert pose to component space
		CS_Output.Pose.InitPose(Output.Pose);
	}
	
	//Record Features
	const int32 ConfigIterations = FMath::Min(MotionConfigs.Num(), MotionRecorderData.Num());
	for(int32 ConfigIndex = 0; ConfigIndex < ConfigIterations; ++ConfigIndex)
	{
		if(UMotionMatchConfig* MotionConfig = MotionConfigs[ConfigIndex])
		{
			FMotionRecordData& MotionRecord = MotionRecorderData[ConfigIndex];
			
			int32 FeatureOffset = 0;
			for(const TObjectPtr<UMatchFeatureBase> Feature : MotionConfig->Features)
			{
				if(Feature->PoseCategory == EPoseCategory::Quality)
				{
					Feature->ExtractRuntime(CS_Output.Pose, &MotionRecord.RecordedPoseArray[FeatureOffset],
						&MotionRecord.FeatureCacheData[FeatureOffset],
						Output.AnimInstanceProxy, PoseDeltaTime);
				}
		
				FeatureOffset += Feature->Size();
			}
		}
	}

#if ENABLE_ANIM_DEBUG && ENABLE_DRAW_DEBUG
	const int32 DebugLevel = CVarMotionSnapshotDebug.GetValueOnAnyThread();
	if (Output.AnimInstanceProxy)
	{
		if (DebugLevel > 0)
		{
			//Todo: Draw Motion Snapshot Debug for Features
		}
	}
#endif
}

void FAnimNode_MotionRecorder::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData);

	const FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);
	Source.GatherDebugData(DebugData);
}

#undef LOCTEXT_NAMESPACE
