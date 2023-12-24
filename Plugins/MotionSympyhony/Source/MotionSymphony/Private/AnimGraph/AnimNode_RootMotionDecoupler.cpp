//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimGraph/AnimNode_RootMotionDecoupler.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimRootMotionProvider.h"

DECLARE_CYCLE_STAT(TEXT("RootMotionDecoupler Eval"), STAT_RootMotionDecoupler_Eval, STATGROUP_Anim);

#if ENABLE_ANIM_DEBUG
//Todo: Add Debugging CVars here
#endif

void FAnimNode_RootMotionDecoupler::GatherDebugData(FNodeDebugData& DebugData)
{
	FAnimNode_SkeletalControlBase::GatherDebugData(DebugData);

	FString DebugLine = DebugData.GetNodeName(this);
#if ENABLE_ANIM_DEBUG
	//Todo: Add custom debug lines here
#endif
	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_RootMotionDecoupler::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);
	CachedDeltaTime = Context.GetDeltaTime();

	// If we just became relevant and haven't been initialized yet, then reset.
	if(!bIsFirstUpdate
		&& UpdateCounter.HasEverBeenUpdated() 
		&& !UpdateCounter.WasSynchronizedCounter(Context.AnimInstanceProxy->GetUpdateCounter()))
	{
		Reset(Context);
	}

	UpdateCounter.SynchronizeWith(Context.AnimInstanceProxy->GetUpdateCounter());
}

void FAnimNode_RootMotionDecoupler::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);
	AnimInstanceProxy = Context.AnimInstanceProxy;
	Reset(Context);
}

void FAnimNode_RootMotionDecoupler::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,
	TArray<FBoneTransform>& OutBoneTransforms)
{
	SCOPE_CYCLE_COUNTER(STAT_RootMotionDecoupler_Eval);
	check(OutBoneTransforms.Num() == 0);

	const UE::Anim::IAnimRootMotionProvider* RootMotionProvider = UE::Anim::IAnimRootMotionProvider::Get();
	if(!RootMotionProvider)
	{
		UE_LOG(LogTemp, Error, TEXT("AnimNode_RootMotionDecoupler: Unable to Evaluate Skeletal Control, RootMotionProvider is null."))
		return;
	}
	
	//Extract the root motion into a transform
	FTransform RootMotionTransformDelta = FTransform::Identity;
	if(RootMotionProvider->ExtractRootMotion(Output.CustomAttributes, RootMotionTransformDelta))
	{
		UE_LOG(LogTemp, Warning, TEXT("Yes Root Motion Attributes"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No Root Motion Attributes"))
	}

	//Have to to do this even though Required bones isn't used
	const FBoneContainer& RequiredBones = Output.Pose.GetPose().GetBoneContainer();
	
	//BoneIndex for the root bone (always at index 0)
	const FCompactPoseBoneIndex TargetBoneIndex(0);
	

	ComponentTransform_WS = AnimInstanceProxy->GetComponentTransform();
	const FTransform RootBoneTransform_CS = Output.Pose.GetComponentSpaceTransform(TargetBoneIndex); 
	const FTransform RootBoneTransform_WS = LastRootTransform_WS;
	//FTransform NewRootBoneTransform_WS;
	FTransform NewRootBoneTransform_CS;

	FTransform ConsumedRootMotion(FTransform::Identity);
	
	//Handle decoupling of the root location
	if(IsDecoupleLocation())
	{
		//Apply the root motion delta to the root bone location of the last frame
		const FVector RootLocationDelta = RootMotionTransformDelta.GetLocation();
		const FVector NewRootBoneLocation_WS = RootBoneTransform_WS.GetLocation() + RootLocationDelta;

		//Todo: Now Clamp it
		switch(GetTranslationClampMode())
		{
		case EDecouplerClampMode::None: {} break;
		case EDecouplerClampMode::Hard: {} break;
		default: break;
		}

		//Todo: Calculate Consumed Root Movement

		//Convert to component space and set the new location on the new root bone transform
		NewRootBoneTransform_CS.SetLocation(ComponentTransform_WS.InverseTransformPosition(NewRootBoneLocation_WS));
	}
	else
	{
		//Do not decouple the rotation, make sure that it is not altered
		NewRootBoneTransform_CS.SetLocation(RootBoneTransform_CS.GetLocation());
	}

	//Handle decoupling of the root rotation
	if(IsDecoupleRotation())
	{
		//Apply the root rotation delta to the root bone rotation of the last frame
		const FQuat RootRotationDelta = RootMotionTransformDelta.GetRotation();
		const FQuat NewRootBoneRotation_WS = RootBoneTransform_WS.GetRotation() * RootRotationDelta;

		//Todo: Now Clamp it
		switch(GetRotationClampMode())
		{
		case EDecouplerClampMode::None: {} break;
		case EDecouplerClampMode::Hard: {} break;
		default: break;
		}

		//Todo: Calculate Consumed Root Movement

		//Convert to component space and set the new rotation on the new root bone transform
		NewRootBoneTransform_CS.SetRotation(ComponentTransform_WS.InverseTransformRotation(NewRootBoneRotation_WS));
	}
	else
	{
		//Do not decouple the rotation, make sure that it is not altered
		NewRootBoneTransform_CS.SetRotation(RootBoneTransform_CS.GetRotation());
	}
	
	//Add the new root bone transform to the OutBoneTransforms array
	OutBoneTransforms.Add(FBoneTransform(TargetBoneIndex, NewRootBoneTransform_CS));

	//Save the world space of the root bone for next update.
	LastRootTransform_WS = RootBoneTransform_WS;
}

bool FAnimNode_RootMotionDecoupler::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
#if ENABLE_ANIM_DEBUG
	//Todo: check enable CVar to disable the node with debugging if desired
#endif

	return Alpha > UE_KINDA_SMALL_NUMBER
		&& (bDecoupleLocation || bDecoupleRotation)
		&& IsLODEnabled(AnimInstanceProxy);
}

bool FAnimNode_RootMotionDecoupler::IsDecoupleLocation() const
{
	return GET_ANIM_NODE_DATA(bool, bDecoupleLocation);
}

EDecouplerClampMode FAnimNode_RootMotionDecoupler::GetTranslationClampMode() const
{
	return GET_ANIM_NODE_DATA(EDecouplerClampMode, TranslationClampMode);
}

float FAnimNode_RootMotionDecoupler::GetTranslationClampDistance() const
{
	return GET_ANIM_NODE_DATA(float, TranslationClampDistance);
}

bool FAnimNode_RootMotionDecoupler::IsDecoupleRotation() const
{
	return GET_ANIM_NODE_DATA(bool, bDecoupleRotation);
}

EDecouplerClampMode FAnimNode_RootMotionDecoupler::GetRotationClampMode() const
{
	return GET_ANIM_NODE_DATA(EDecouplerClampMode, RotationClampMode);
}

float FAnimNode_RootMotionDecoupler::GetRotationClampDegrees() const
{
	return GET_ANIM_NODE_DATA(float, RotationClampDegrees);
}

void FAnimNode_RootMotionDecoupler::Reset(const FAnimationBaseContext& Context)
{
	ComponentTransform_WS = Context.AnimInstanceProxy->GetComponentTransform();
	LastRootTransform_WS = ComponentTransform_WS;
	bIsFirstUpdate = true;
}






