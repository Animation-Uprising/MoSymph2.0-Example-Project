//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MSFootLockerData.h"
#include "MSFootLockerMath.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"

FMSFootLockLimbDefinition::FMSFootLockLimbDefinition()
	: LimbBoneCount(2),
	LengthSqr(-1.0f),
	Length(-1.0f),
	HeightDelta(0.0f),
	ToeLocation_LS(FVector::ZeroVector),
	ToeLocation_CS(FVector::ZeroVector),
	FootLocation_CS(FVector::ZeroVector),
	BaseBoneTransform_CS(FTransform::Identity)
{
}

void FMSFootLockLimbDefinition::Initialize(const FBoneContainer& PoseBones, const FAnimInstanceProxy* InAnimInstanceProxy)
{
	Length = -1.0f;

	//Initialize set bones
	FootBone.Initialize(PoseBones);
	ToeBone.Initialize(PoseBones);
	IkTarget.Initialize(PoseBones);

	Bones.Empty(LimbBoneCount);

	if (!FootBone.IsValidToEvaluate(PoseBones))
	{
		return;
	}

	const FReferenceSkeleton& RefSkeleton = PoseBones.GetReferenceSkeleton();

	int32 ParentBoneIndex = PoseBones.GetParentBoneIndex(FootBone.BoneIndex);
	for (int32 i = 0; i < LimbBoneCount; ++i)
	{
		if(ParentBoneIndex == 0)
		{
			break;
		}

		FBoneReference Bone = FBoneReference(RefSkeleton.GetBoneName(ParentBoneIndex));
		Bone.Initialize(PoseBones);
		Bones.Add(Bone);

		ParentBoneIndex = PoseBones.GetParentBoneIndex(ParentBoneIndex);
	}
}

bool FMSFootLockLimbDefinition::IsValid(const FBoneContainer& PoseBones)
{
	bool bIsValid = FootBone.IsValidToEvaluate(PoseBones)
		&& ToeBone.IsValidToEvaluate(PoseBones)
		&& IkTarget.IsValidToEvaluate(PoseBones)
		&& (Bones.Num() == LimbBoneCount)
		&& (LimbBoneCount > 1);

	for(FBoneReference& Bone : Bones)
	{
		if (!Bone.IsValidToEvaluate(PoseBones))
		{
			bIsValid = false;
			break;
		}
	}

	return bIsValid;
}

void FMSFootLockLimbDefinition::CalculateLength(FCSPose<FCompactPose>& Pose)
{
	//Length from foot to previous bone hinge
	Length = Pose.GetLocalSpaceTransform(FootBone.CachedCompactPoseIndex).GetLocation().Size();

	//Length from subsequent bones to their previous bone up the chain (Base bone excluded)
	for (int32 i = 0; i < LimbBoneCount - 1; ++i)
	{
		Length += Pose.GetLocalSpaceTransform(Bones[i].CachedCompactPoseIndex).GetLocation().Size();
	}
	
	LengthSqr = Length * Length;
}

void FMSFootLockLimbDefinition::CalculateFootToToeOffset(FCSPose<FCompactPose>& Pose)
{
	ToeLocation_LS = Pose.GetLocalSpaceTransform(ToeBone.CachedCompactPoseIndex).GetLocation();
}
