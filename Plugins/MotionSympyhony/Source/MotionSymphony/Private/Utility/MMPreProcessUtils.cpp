//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MMPreProcessUtils.h"

#include "JointData.h"
#include "TrajectoryPoint.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Enumerations/EMotionMatchingEnums.h"
#include "Animation/MirrorDataTable.h"

void FMMPreProcessUtils::ExtractRootMotionParams(FRootMotionMovementParams& OutRootMotion, 
	const TArray<FBlendSampleData>& BlendSampleData, const float BaseTime, const float DeltaTime, const bool AllowLooping)
{
	for (const FBlendSampleData& Sample : BlendSampleData)
	{
		const float SampleWeight = Sample.GetClampedWeight();

		if (SampleWeight > 0.0001f)
		{
			FTransform RootDeltaTransform = Sample.Animation->ExtractRootMotion(BaseTime, DeltaTime, AllowLooping);
			RootDeltaTransform.NormalizeRotation();
			OutRootMotion.AccumulateWithBlend(RootDeltaTransform, SampleWeight);
		}
	}
}

void FMMPreProcessUtils::ExtractRootVelocity(FVector& OutRootVelocity, float& OutRootRotVelocity,
	UAnimSequence* AnimSequence, const float Time, const float PoseInterval)
{
	if (!AnimSequence)
	{
		OutRootVelocity = FVector::ZeroVector;
		OutRootRotVelocity = 0.0f;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);
	
	FTransform RootDeltaTransform = AnimSequence->ExtractRootMotion(StartTime, PoseInterval, false);
	RootDeltaTransform.NormalizeRotation();
	const FVector RootDeltaPos = RootDeltaTransform.GetTranslation();
	
	OutRootRotVelocity = RootDeltaTransform.GetRotation().Euler().Z / PoseInterval;
	OutRootVelocity = RootDeltaPos.GetSafeNormal() * (RootDeltaPos.Size() / PoseInterval);
}

void FMMPreProcessUtils::ExtractRootVelocity(FVector& OutRootVelocity, float& OutRootRotVelocity, 
	const TArray<FBlendSampleData>& BlendSampleData, const float Time, const float PoseInterval)
{
	if (BlendSampleData.Num() == 0)
	{
		OutRootVelocity = FVector::ZeroVector;
		OutRootRotVelocity = 0.0f;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);

	FRootMotionMovementParams RootMotionParams;
	RootMotionParams.Clear();

	ExtractRootMotionParams(RootMotionParams, BlendSampleData, StartTime, PoseInterval, false);

	FTransform RootDeltaTransform = RootMotionParams.GetRootMotionTransform();
	RootDeltaTransform.NormalizeRotation();
	const FVector RootDeltaPos = RootDeltaTransform.GetTranslation();

	OutRootRotVelocity = RootDeltaTransform.GetRotation().Euler().Z / PoseInterval;
	OutRootVelocity = RootDeltaPos.GetSafeNormal() * (RootDeltaPos.Size() / PoseInterval);
}

void FMMPreProcessUtils::ExtractRootVelocity(FVector& OutRootVelocity, float& OutRootRotVelocity, 
	UAnimComposite* AnimComposite, const float Time, const float PoseInterval)
{
	if (!AnimComposite)
	{
		OutRootVelocity = FVector::ZeroVector;
		OutRootRotVelocity = 0.0f;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);

	FRootMotionMovementParams RootMotionParams;
	AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, StartTime, StartTime + PoseInterval, RootMotionParams);

	FTransform RootDeltaTransform = RootMotionParams.GetRootMotionTransform();
	RootDeltaTransform.NormalizeRotation();
	const FVector RootDeltaPos = RootDeltaTransform.GetTranslation();

	OutRootRotVelocity = RootDeltaTransform.GetRotation().Euler().Z / PoseInterval;
	OutRootVelocity = RootDeltaPos.GetSafeNormal() * (RootDeltaPos.Size() / PoseInterval);
}

void FMMPreProcessUtils::ExtractPastTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, 
	UAnimSequence* AnimSequence, const float BaseTime, const float PointTime, 
	ETrajectoryPreProcessMethod PastMethod, UAnimSequence* PrecedingMotion)
{
	if (!AnimSequence)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	const float PointAnimTime = BaseTime + PointTime;

	//Root delta to the beginning of the clip
	FTransform RootDelta;

	if (static_cast<int32>(PastMethod) > static_cast<int32>(ETrajectoryPreProcessMethod::IgnoreEdges) 
		&& PointAnimTime < 0.0f)
	{
		//Trajectory point Time is outside the bounds of the clip and we are not ignoring edges
		RootDelta = AnimSequence->ExtractRootMotion(BaseTime, -BaseTime, false);
		
		switch (PastMethod)
		{
			//Extrapolate the motion at the beginning of the clip
			case ETrajectoryPreProcessMethod::Extrapolate:
			{
				const FTransform initialMotion = AnimSequence->ExtractRootMotion(0.05f, -0.05f, false);

				//transform the root delta by initial motion for a required number of Iterations
				const int32 Iterations = FMath::RoundToInt(FMath::Abs(PointAnimTime) / 0.05f);
				for (int32 i = 0; i < Iterations; ++i)
				{
					RootDelta *= initialMotion;
				}

			} break;

			case ETrajectoryPreProcessMethod::Animation:
			{
				if (PrecedingMotion == nullptr)
					break;

				const FTransform precedingRootDelta = PrecedingMotion->ExtractRootMotion(PrecedingMotion->GetPlayLength(), PointAnimTime, false);

				RootDelta *= precedingRootDelta;

			} break;
		}
	}
	else
	{
		//Here the trajectory point either falls within the clip or we are ignoring edges
		//therefore, no fanciness is required
		const float DeltaTime = FMath::Clamp(PointTime, -BaseTime, 0.0f);

		RootDelta = AnimSequence->ExtractRootMotion(BaseTime, DeltaTime, false);
	}

	//Apply the calculated root deltas
	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractPastTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, const TArray<FBlendSampleData>& BlendSampleData, 
	const float BaseTime, const float PointTime, ETrajectoryPreProcessMethod PastMethod, UAnimSequence* PrecedingMotion)
{
	if (BlendSampleData.Num() == 0)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	float PointAnimTime = BaseTime + PointTime;

	//Root delta to the beginning of the clip
	FRootMotionMovementParams RootMotionParams;
	RootMotionParams.Clear();

	FTransform RootDelta;

	if (static_cast<int32>(PastMethod) > static_cast<int32>(ETrajectoryPreProcessMethod::IgnoreEdges)
		&& PointAnimTime < 0.0f)
	{
		ExtractRootMotionParams(RootMotionParams, BlendSampleData, BaseTime, -BaseTime, false);

		//Trajectory point Time is outside the bounds of the clip and we are not ignoring edges
		RootDelta = RootMotionParams.GetRootMotionTransform();

		switch (PastMethod)
		{
			//Extrapolate the motion at the beginning of the clip
			case ETrajectoryPreProcessMethod::Extrapolate:
			{
				FRootMotionMovementParams ExtrapRootMotionParams;
				ExtrapRootMotionParams.Clear();

				ExtractRootMotionParams(ExtrapRootMotionParams, BlendSampleData, 0.05f, - 0.05f, false);
				FTransform InitialMotion = ExtrapRootMotionParams.GetRootMotionTransform();
				InitialMotion.NormalizeRotation();

				//transform the root delta by initial motion for a required number of Iterations
				int32 Iterations = FMath::RoundToInt(FMath::Abs(PointAnimTime) / 0.05f);
				for (int32 i = 0; i < Iterations; ++i)
				{
					RootDelta *= InitialMotion;
				}

			} break;

			case ETrajectoryPreProcessMethod::Animation:
			{
				if (PrecedingMotion == nullptr)
					break;


				FTransform precedingRootDelta = PrecedingMotion->ExtractRootMotion(PrecedingMotion->GetPlayLength(), PointAnimTime, false);

				RootDelta *= precedingRootDelta;

			} break;
		default: ;
		}
	}
	else
	{
		//Here the trajectory point either falls within the clip or we are ignoring edges
		//therefore, no fanciness is required
		float DeltaTime = FMath::Clamp(PointTime, -BaseTime, 0.0f);

		ExtractRootMotionParams(RootMotionParams, BlendSampleData, BaseTime, DeltaTime, false);
		RootDelta = RootMotionParams.GetRootMotionTransform();
		RootDelta.NormalizeRotation();
	}

	//Apply the calculated root deltas
	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractPastTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, UAnimComposite* AnimComposite, 
	const float BaseTime, const float PointTime, ETrajectoryPreProcessMethod PastMethod, UAnimSequence* PrecedingMotion)
{
	if (!AnimComposite)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	float PointAnimTime = BaseTime + PointTime;

	//Root delta to the beginning of the clip
	FTransform RootDelta;

	if (static_cast<int32>(PastMethod) > static_cast<int32>(ETrajectoryPreProcessMethod::IgnoreEdges)
		&& PointAnimTime < 0.0f)
	{
		//Trajectory point Time is outside the bounds of the clip and we are not ignoring edges
		FRootMotionMovementParams RootMotionParams;
		AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, BaseTime, 0.0f, RootMotionParams);
		RootDelta = RootMotionParams.GetRootMotionTransform();

		switch (PastMethod)
		{
			//Extrapolate the motion at the beginning of the clip
		case ETrajectoryPreProcessMethod::Extrapolate:
		{
			RootMotionParams.Clear();
			AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, 0.05f, 0.0f, RootMotionParams);
			FTransform InitialMotion = RootMotionParams.GetRootMotionTransform();

			//Transform the root delta by initial motion for a required number of Iterations
			int32 Iterations = FMath::RoundToInt(FMath::Abs(PointAnimTime) / 0.05f);
			for (int32 i = 0; i < Iterations; ++i)
			{
				RootDelta *= InitialMotion;
			}

		} break;

		case ETrajectoryPreProcessMethod::Animation:
		{
			if (PrecedingMotion == nullptr)
				break;

			FTransform PrecedingRootDelta = PrecedingMotion->ExtractRootMotion(PrecedingMotion->GetPlayLength(), PointAnimTime, false);

			RootDelta *= PrecedingRootDelta;

		} break;
		default: ;
		}
	}
	else
	{
		//Here the trajectory point either falls within the clip or we are ignoring edges
		//therefore, no fanciness is required
		float DeltaTime = FMath::Clamp(PointTime, -BaseTime, 0.0f);

		FRootMotionMovementParams RootMotionParams;
		AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, BaseTime, BaseTime + DeltaTime, RootMotionParams);
		RootDelta = RootMotionParams.GetRootMotionTransform();
	}

	//Apply the calculated root deltas
	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractFutureTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, 
	UAnimSequence* AnimSequence, const float BaseTime, const float PointTime, 
	ETrajectoryPreProcessMethod FutureMethod, UAnimSequence* FollowingMotion)
{
	if (!AnimSequence)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	const float PointAnimTime = BaseTime + PointTime;

	//Root delta to the beginning of the clip
	FTransform RootDelta;

	if (static_cast<int32>(FutureMethod) > static_cast<int32>(ETrajectoryPreProcessMethod::IgnoreEdges)
		&& PointAnimTime > AnimSequence->GetPlayLength())
	{
		//Trajectory point Time is outside the bounds of the clip and we are not ignoring edges

		RootDelta = AnimSequence->ExtractRootMotion(BaseTime, AnimSequence->GetPlayLength() - BaseTime, false);

		switch (FutureMethod)
		{
			//Extrapolate the motion at the end of the clip
			case ETrajectoryPreProcessMethod::Extrapolate:
			{
				const FTransform EndMotion = AnimSequence->ExtractRootMotion(AnimSequence->GetPlayLength() - 0.05f, 0.05f, false);

				//transform the root delta by initial motion for a required number of Iterations
				const int32 Iterations = FMath::RoundToInt(FMath::Abs(PointAnimTime - AnimSequence->GetPlayLength()) / 0.05f);
				for (int32 i = 0; i < Iterations; ++i)
				{
					RootDelta *= EndMotion;
				}

			} break;

			case ETrajectoryPreProcessMethod::Animation:
			{
				if (FollowingMotion == nullptr)
				{
					break;
				}

				FTransform FollowingRootDelta = FollowingMotion->ExtractRootMotion(0.0f, PointAnimTime - AnimSequence->GetPlayLength(), false);

				RootDelta *= FollowingRootDelta;

			} break;
		default: ;
		}
	}
	else
	{
		//Here the trajectory point either falls within the clip or we are ignoring edges
		//therefore, no fanciness is required
		const float Time = FMath::Clamp(PointTime, 0.0f, AnimSequence->GetPlayLength() - BaseTime);

		RootDelta = AnimSequence->ExtractRootMotion(BaseTime, Time, false);
	}

	//Apply the calculated root deltas
	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractFutureTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, const TArray<FBlendSampleData>& BlendSampleData, 
	const float BaseTime, const float PointTime, ETrajectoryPreProcessMethod FutureMethod, UAnimSequence* FollowingMotion)
{
	if(BlendSampleData.Num() == 0)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	float PointAnimTime = BaseTime + PointTime;
	float AnimLength = BlendSampleData[0].Animation->GetPlayLength();

	//Root delta to the beginning of the clip
	FRootMotionMovementParams RootMotionParams;
	RootMotionParams.Clear();
	FTransform RootDelta;

	
	if (static_cast<int32>(FutureMethod) > static_cast<int32>(ETrajectoryPreProcessMethod::IgnoreEdges)
		&& PointAnimTime > AnimLength)
	{
		//Trajectory point Time is outside the bounds of the clip and we are not ignoring edges
		ExtractRootMotionParams(RootMotionParams, BlendSampleData, BaseTime, AnimLength - BaseTime, false);

		RootDelta = RootMotionParams.GetRootMotionTransform();

		switch (FutureMethod)
		{
			//Extrapolate the motion at the end of the clip
		case ETrajectoryPreProcessMethod::Extrapolate:
		{
			FRootMotionMovementParams ExtrapRootMotionParams;
			ExtrapRootMotionParams.Clear();
			ExtractRootMotionParams(ExtrapRootMotionParams, BlendSampleData, AnimLength - 0.05f, 0.05f, false);

			FTransform EndMotion = ExtrapRootMotionParams.GetRootMotionTransform();
			EndMotion.NormalizeRotation();

			//transform the root delta by initial motion for a required number of Iterations
			int32 Iterations = FMath::RoundToInt(FMath::Abs(PointAnimTime - AnimLength) / 0.05f);
			for (int32 i = 0; i < Iterations; ++i)
			{
				RootDelta *= EndMotion;
			}

		} break;

		case ETrajectoryPreProcessMethod::Animation:
		{
			if (FollowingMotion == nullptr)
				break;

			FTransform FollowingRootDelta = FollowingMotion->ExtractRootMotion(0.0f, PointAnimTime - AnimLength, false);

			RootDelta *= FollowingRootDelta;

		} break;
		default: ;
		}
	}
	else
	{
		//Here the trajectory point either falls within the clip or we are ignoring edges
		//therefore, no fanciness is required
		float DeltaTime = FMath::Clamp(PointTime, 0.0f, AnimLength - BaseTime);
		
		ExtractRootMotionParams(RootMotionParams, BlendSampleData, BaseTime, DeltaTime, false);
		RootDelta = RootMotionParams.GetRootMotionTransform();
		
	}

	RootDelta.NormalizeRotation();

	//Apply the calculated root deltas
	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractFutureTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, UAnimComposite* AnimComposite, 
	const float BaseTime, const float PointTime, ETrajectoryPreProcessMethod FutureMethod, UAnimSequence* FollowingMotion)
{
	if (!AnimComposite)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	float PointAnimTime = BaseTime + PointTime;

	//Root delta to the beginning of the clip
	FTransform RootDelta;

	float SequenceLength = AnimComposite->GetPlayLength();
	if (static_cast<int32>(FutureMethod) > static_cast<int32>(ETrajectoryPreProcessMethod::IgnoreEdges)
		&& PointAnimTime > SequenceLength)
	{
		//Trajectory point Time is outside the bounds of the clip and we are not ignoring edges
		FRootMotionMovementParams RootMotionParams;
		AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, BaseTime, SequenceLength, RootMotionParams);
		RootDelta = RootMotionParams.GetRootMotionTransform();

		switch (FutureMethod)
		{
			//Extrapolate the motion at the end of the clip
		case ETrajectoryPreProcessMethod::Extrapolate:
		{
			RootMotionParams.Clear();
			AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, SequenceLength - 0.05f,
				SequenceLength, RootMotionParams);

			FTransform EndMotion = RootMotionParams.GetRootMotionTransform();

			//transform the root delta by initial motion for a required number of Iterations
			int32 Iterations = FMath::RoundToInt(FMath::Abs(PointAnimTime - SequenceLength) / 0.05f);
			for (int32 i = 0; i < Iterations; ++i)
			{
				RootDelta *= EndMotion;
			}

		} break;

		case ETrajectoryPreProcessMethod::Animation:
		{
			if (FollowingMotion == nullptr)
				break;

			FTransform FollowingRootDelta = FollowingMotion->ExtractRootMotion(0.0f, PointAnimTime - SequenceLength, false);

			RootDelta *= FollowingRootDelta;

		} break;
		default: ;
		}
	}
	else
	{
		//Here the trajectory point either falls within the clip or we are ignoring edges
		//therefore, no fanciness is required
		 float Time = FMath::Clamp(PointTime, 0.0f, SequenceLength - BaseTime);

		FRootMotionMovementParams RootMotionParams;
		AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, BaseTime, BaseTime + Time, RootMotionParams);
		RootDelta = RootMotionParams.GetRootMotionTransform();
	}

	//Apply the calculated root deltas
	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractLoopingTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, 
	UAnimSequence* AnimSequence, const float BaseTime, const float PointTime)
{
	if (!AnimSequence)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	FTransform RootDelta = AnimSequence->ExtractRootMotion(BaseTime, PointTime, true);
	RootDelta.NormalizeRotation();

	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractLoopingTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, const TArray<FBlendSampleData>& BlendSampleData, 
	const float BaseTime, const float PointTime)
{
	if (BlendSampleData.Num() == 0)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	FRootMotionMovementParams RootMotionParams;
	RootMotionParams.Clear();

	ExtractRootMotionParams(RootMotionParams, BlendSampleData, BaseTime, PointTime, true);

	FTransform RootDelta = RootMotionParams.GetRootMotionTransform();
	RootDelta.NormalizeRotation();

	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractLoopingTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, 
	UAnimComposite* AnimComposite, const float BaseTime, const float PointTime)
{
	if (!AnimComposite)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	const float PointAnimTime = BaseTime + PointTime;

	FRootMotionMovementParams RootMotionParams;
	AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, BaseTime, PointAnimTime, RootMotionParams);

	const float SequenceLength = AnimComposite->GetPlayLength();
	if (PointAnimTime < 0) //Extract from the previous loop and accumulate
	{
		FRootMotionMovementParams PastRootMotionParams;
		AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, SequenceLength, 
			SequenceLength + PointAnimTime, PastRootMotionParams);

		RootMotionParams.Accumulate(PastRootMotionParams);
	}
	else if (PointAnimTime > SequenceLength) //Extract from the next loop and accumulate
	{
		FRootMotionMovementParams FutureRootMotionParams;
		AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, 0.0f, 
			PointAnimTime - SequenceLength, FutureRootMotionParams);

		RootMotionParams.Accumulate(FutureRootMotionParams);
	}

	FTransform RootDelta = RootMotionParams.GetRootMotionTransform();
	
	RootDelta.NormalizeRotation();

	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractJointData(FJointData& OutJointData, 
	UAnimSequence* AnimSequence, const int32 JointId, const float Time, const float PoseInterval)
{
	if (!AnimSequence)
	{
		OutJointData = FJointData();
		return;
	}

	FTransform JointTransform = FTransform::Identity;
	GetJointTransform_RootRelative(JointTransform, AnimSequence, JointId, Time);

	FVector JointVelocity = FVector::ZeroVector;
	GetJointVelocity_RootRelative(JointVelocity, AnimSequence, JointId, Time, PoseInterval);

	OutJointData = FJointData(JointTransform.GetLocation(), JointVelocity);
}

void FMMPreProcessUtils::ExtractJointData(FJointData& OutJointData, const TArray<FBlendSampleData>& BlendSampleData,
	const int32 JointId, const float Time, const float PoseInterval)
{
	if(BlendSampleData.Num() == 0)
	{
		OutJointData = FJointData();
		return;
	}

	FTransform JointTransform = FTransform::Identity;
	GetJointTransform_RootRelative(JointTransform, BlendSampleData, JointId, Time);

	FVector JointVelocity = FVector::ZeroVector;
	GetJointVelocity_RootRelative(JointVelocity, BlendSampleData, JointId, Time, PoseInterval);

	OutJointData = FJointData(JointTransform.GetLocation(), JointVelocity);
}

void FMMPreProcessUtils::ExtractJointData(FJointData& OutJointData, UAnimComposite* AnimComposite,
	const int32 JointId, const float Time, const float PoseInterval)
{
	if (!AnimComposite)
	{
		OutJointData = FJointData();
		return;
	}

	FTransform JointTransform = FTransform::Identity;
	GetJointTransform_RootRelative(JointTransform, AnimComposite, JointId, Time);

	FVector JointVelocity = FVector::ZeroVector;
	GetJointVelocity_RootRelative(JointVelocity, AnimComposite, JointId, Time, PoseInterval);

	OutJointData = FJointData(JointTransform.GetLocation(), JointVelocity);
}

void FMMPreProcessUtils::ExtractJointData(FJointData& OutJointData, UAnimSequence* AnimSequence,
	 const FBoneReference& BoneReference, const float Time, const float PoseInterval)
{
	if(!AnimSequence)
	{
		OutJointData = FJointData();
		return;
	}

	TArray<FName> BonesToRoot;
	FindBonePathToRoot(AnimSequence, BoneReference.BoneName, BonesToRoot);
	if(BonesToRoot.Num() > 0)
    {
        BonesToRoot.RemoveAt(BonesToRoot.Num() - 1); //Removes the root
    }
	
	FTransform JointTransform_CS = FTransform::Identity;
	GetJointTransform_RootRelative(JointTransform_CS, AnimSequence, BonesToRoot, Time);
	
	FVector JointVelocity_CS = FVector::ZeroVector;
	GetJointVelocity_RootRelative(JointVelocity_CS, AnimSequence, BonesToRoot, Time, PoseInterval);

	OutJointData = FJointData(JointTransform_CS.GetLocation(), JointVelocity_CS);
}

void FMMPreProcessUtils::ExtractJointData(FJointData& OutJointData, const TArray<FBlendSampleData>& BlendSampleData, 
	const FBoneReference& BoneReference, const float Time, const float PoseInterval)
{
	if (BlendSampleData.Num() == 0)
	{
		OutJointData = FJointData();
		return;
	}

	TArray<FName> BonesToRoot;
	FindBonePathToRoot(BlendSampleData[0].Animation, BoneReference.BoneName, BonesToRoot);
	if(BonesToRoot.Num() > 0)
    {
        BonesToRoot.RemoveAt(BonesToRoot.Num() - 1); //Removes the root
    }

	FTransform JointTransform_CS = FTransform::Identity;
	GetJointTransform_RootRelative(JointTransform_CS, BlendSampleData, BonesToRoot, Time);
	//JointTransform_CS.SetLocation(JointTransform_CS.GetLocation() - RootBoneTransform.GetLocation());

	FVector JointVelocity_CS = FVector::ZeroVector;
	GetJointVelocity_RootRelative(JointVelocity_CS, BlendSampleData, BonesToRoot, Time, PoseInterval);

	OutJointData = FJointData(JointTransform_CS.GetLocation(), JointVelocity_CS);
}

void FMMPreProcessUtils::ExtractJointData(FJointData& OutJointData, UAnimComposite* AnimComposite, 
	const FBoneReference& BoneReference, const float Time, const float PoseInterval)
{
	if (!AnimComposite || AnimComposite->AnimationTrack.AnimSegments.Num() == 0)
	{
		OutJointData = FJointData();
		return;
	}

	UAnimSequence* CompositeFirstSequence = Cast<UAnimSequence>(AnimComposite->AnimationTrack.AnimSegments[0].GetAnimReference());

	if (!CompositeFirstSequence)
	{
		OutJointData = FJointData();
		return;
	}

	TArray<FName> BonesToRoot;
	FindBonePathToRoot(CompositeFirstSequence, BoneReference.BoneName, BonesToRoot);
	if(BonesToRoot.Num() > 0)
    {
        BonesToRoot.RemoveAt(BonesToRoot.Num() - 1); //Removes the root
    }

	FTransform JointTransform_CS = FTransform::Identity;
	GetJointTransform_RootRelative(JointTransform_CS, AnimComposite, BonesToRoot, Time);

	FVector JointVelocity_CS = FVector::ZeroVector;
	GetJointVelocity_RootRelative(JointVelocity_CS, AnimComposite, BonesToRoot, Time, PoseInterval);

	OutJointData = FJointData(JointTransform_CS.GetLocation(), JointVelocity_CS);
}

void FMMPreProcessUtils::GetJointVelocity_RootRelative(FVector & OutJointVelocity, 
	UAnimSequence * AnimSequence, const int32 JointId, const float Time, const float PoseInterval)
{
	if(!AnimSequence)
	{
		OutJointVelocity = FVector::ZeroVector;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);

	FTransform BeforeTransform = FTransform::Identity;
	GetJointTransform_RootRelative(BeforeTransform, AnimSequence, JointId, StartTime);

	FTransform AfterTransform = FTransform::Identity;
	GetJointTransform_RootRelative(AfterTransform, AnimSequence, JointId, StartTime + PoseInterval);

	OutJointVelocity = (AfterTransform.GetLocation() - BeforeTransform.GetLocation()) / PoseInterval;
}

void FMMPreProcessUtils::GetJointVelocity_RootRelative(FVector& OutJointVelocity, 
	const TArray<FBlendSampleData>& BlendSampleData, const int32 JointId, const float Time, const float PoseInterval)
{
	if(BlendSampleData.Num() == 0)
	{
		OutJointVelocity = FVector::ZeroVector;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);

	FTransform BeforeTransform = FTransform::Identity;
	GetJointTransform_RootRelative(BeforeTransform, BlendSampleData, JointId, StartTime);

	FTransform AfterTransform = FTransform::Identity;
	GetJointTransform_RootRelative(AfterTransform, BlendSampleData, JointId, StartTime + PoseInterval);

	OutJointVelocity = (AfterTransform.GetLocation() - BeforeTransform.GetLocation()) / PoseInterval;
}

void FMMPreProcessUtils::GetJointVelocity_RootRelative(FVector& OutJointVelocity, UAnimComposite* AnimComposite,
	const int32 JointId, const float Time, const float PoseInterval)
{
	if (!AnimComposite)
	{
		OutJointVelocity = FVector::ZeroVector;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);

	FTransform BeforeTransform = FTransform::Identity;
	GetJointTransform_RootRelative(BeforeTransform, AnimComposite, JointId, StartTime);

	FTransform AfterTransform = FTransform::Identity;
	GetJointTransform_RootRelative(AfterTransform, AnimComposite, JointId, StartTime + PoseInterval);

	OutJointVelocity = (AfterTransform.GetLocation() - BeforeTransform.GetLocation()) / PoseInterval;
}

void FMMPreProcessUtils::GetJointVelocity_RootRelative(FVector& OutJointVelocity, UAnimSequence* AnimSequence, 
	const TArray<FName>& BonesToRoot, const float Time, const float PoseInterval)
{
	if(!AnimSequence)
	{
		OutJointVelocity = FVector::ZeroVector;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);

	FTransform BeforeTransform = FTransform::Identity;
	GetJointTransform_RootRelative(BeforeTransform, AnimSequence, BonesToRoot, StartTime);

	FTransform AfterTransform = FTransform::Identity;
	GetJointTransform_RootRelative(AfterTransform, AnimSequence, BonesToRoot, StartTime + PoseInterval);

	OutJointVelocity = (AfterTransform.GetLocation() - BeforeTransform.GetLocation()) / PoseInterval;
}

void FMMPreProcessUtils::GetJointVelocity_RootRelative(FVector& OutJointVelocity, const TArray<FBlendSampleData>& BlendSampleData, 
	const TArray<FName>& BonesToRoot, const float Time, const float PoseInterval)
{
	if (BlendSampleData.Num() == 0)
	{
		OutJointVelocity = FVector::ZeroVector;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);

	FTransform BeforeTransform = FTransform::Identity;
	GetJointTransform_RootRelative(BeforeTransform, BlendSampleData, BonesToRoot, StartTime);

	FTransform AfterTransform = FTransform::Identity;
	GetJointTransform_RootRelative(AfterTransform, BlendSampleData, BonesToRoot, StartTime + PoseInterval);

	OutJointVelocity = (AfterTransform.GetLocation() - BeforeTransform.GetLocation()) / PoseInterval;
}

void FMMPreProcessUtils::GetJointVelocity_RootRelative(FVector& OutJointVelocity, UAnimComposite* AnimComposite,
	const TArray<FName>& BonesToRoot, const float Time, const float PoseInterval)
{
	if (!AnimComposite)
	{
		OutJointVelocity = FVector::ZeroVector;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);

	FTransform BeforeTransform = FTransform::Identity;
	GetJointTransform_RootRelative(BeforeTransform, AnimComposite, BonesToRoot, StartTime);

	FTransform AfterTransform = FTransform::Identity;
	GetJointTransform_RootRelative(AfterTransform, AnimComposite, BonesToRoot, StartTime + PoseInterval);

	OutJointVelocity = (AfterTransform.GetLocation() - BeforeTransform.GetLocation()) / PoseInterval;
}


int32 FMMPreProcessUtils::ConvertRefSkelBoneIdToAnimBoneId(const int32 BoneId, 
	const FReferenceSkeleton& FromRefSkeleton, const UAnimSequence* ToAnimSequence)
{
#if WITH_EDITOR
	if (ToAnimSequence || BoneId != INDEX_NONE)
	{
		return ToAnimSequence->CompressedData.GetTrackIndexFromSkeletonIndex(BoneId);
	}
#endif
	return INDEX_NONE;
}

int32 FMMPreProcessUtils::ConvertBoneNameToAnimBoneId(const FName BoneName, const UAnimSequence* ToAnimSequence)
{
#if WITH_EDITOR
	return ToAnimSequence->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(BoneName);


#else
	return INDEX_NONE;
#endif
}

void FMMPreProcessUtils::GetJointTransform_RootRelative(FTransform & OutJointTransform,
	UAnimSequence* AnimSequence, const int32 JointId, const float Time)
{
	OutJointTransform = FTransform::Identity;

	if (!AnimSequence || JointId == INDEX_NONE || JointId == 0)
	{
		return;
	}

	const FReferenceSkeleton RefSkeleton = AnimSequence->GetSkeleton()->GetReferenceSkeleton();

	if (RefSkeleton.IsValidIndex(JointId))
	{
		AnimSequence->GetBoneTransform(OutJointTransform, FSkeletonPoseBoneIndex(JointId), Time, true);
		int32 CurrentJointId = JointId;

		while (RefSkeleton.GetRawParentIndex(CurrentJointId) != 0)
		{
			//Need to get parents by name
			const int32 ParentJointId = RefSkeleton.GetRawParentIndex(CurrentJointId);
			
			FTransform ParentTransform;
			AnimSequence->GetBoneTransform(ParentTransform, FSkeletonPoseBoneIndex(ParentJointId), Time, true);

			OutJointTransform = OutJointTransform * ParentTransform;
			CurrentJointId = ParentJointId;
		}
	}
	else
	{
		OutJointTransform = FTransform::Identity;
	}
}

void FMMPreProcessUtils::GetJointTransform_RootRelative(FTransform& OutJointTransform, 
	const TArray<FBlendSampleData>& BlendSampleData, const int32 JointId, const float Time)
{
	OutJointTransform = FTransform::Identity;
	
	if (BlendSampleData.Num() == 0 || JointId == INDEX_NONE)
	{
		return;
	}

	for(const FBlendSampleData& Sample : BlendSampleData)
	{
		if(!Sample.Animation)
		{
			continue;
		}

		const FReferenceSkeleton& RefSkeleton = Sample.Animation->GetSkeleton()->GetReferenceSkeleton();

		if (!RefSkeleton.IsValidIndex(JointId))
		{
			continue;
		}
		
		const ScalarRegister VSampleWeight(Sample.GetClampedWeight());
		
		FTransform AnimJointTransform;
		Sample.Animation->GetBoneTransform(AnimJointTransform, FSkeletonPoseBoneIndex(JointId), Time, true);

		int32 CurrentJointId = JointId;

		if (CurrentJointId == 0)
		{
			OutJointTransform.Accumulate(AnimJointTransform, VSampleWeight);
			continue;
		}

		int32 ParentJointId = RefSkeleton.GetRawParentIndex(CurrentJointId);
		while (ParentJointId != 0)
		{
			FTransform ParentTransform;
			Sample.Animation->GetBoneTransform(ParentTransform, FSkeletonPoseBoneIndex(ParentJointId), Time, true);

			AnimJointTransform = AnimJointTransform * ParentTransform;
			CurrentJointId = ParentJointId;
			ParentJointId = RefSkeleton.GetRawParentIndex(CurrentJointId);
		}

		OutJointTransform.Accumulate(AnimJointTransform, VSampleWeight);
	}

	OutJointTransform.NormalizeRotation();
}

void FMMPreProcessUtils::GetJointTransform_RootRelative(FTransform& OutJointTransform, 
	UAnimComposite* AnimComposite, const int32 JointId, const float Time)
{
	OutJointTransform = FTransform::Identity;

	if (!AnimComposite || JointId == INDEX_NONE || JointId == 0)
	{
		return;
	}

	float CumDuration = 0.0f;
	UAnimSequence* Sequence = nullptr;
	float NewTime = Time;
	for (int32 i = 0; i < AnimComposite->AnimationTrack.AnimSegments.Num(); ++i)
	{
		const FAnimSegment& AnimSegment = AnimComposite->AnimationTrack.AnimSegments[i];
		const float Length = AnimSegment.GetAnimReference()->GetPlayLength();

		if (Length + CumDuration > Time)
		{
			Sequence = Cast<UAnimSequence>(AnimSegment.GetAnimReference());
			break;
		}
		else
		{
			CumDuration += Length;
			NewTime -= Length;
		}
	}

	if (!Sequence)
	{
		return;
	}

	const FReferenceSkeleton RefSkeleton = Sequence->GetSkeleton()->GetReferenceSkeleton();

	if (RefSkeleton.IsValidIndex(JointId))
	{
		Sequence->GetBoneTransform(OutJointTransform, FSkeletonPoseBoneIndex(JointId), Time, true);
		int32 CurrentJointId = JointId;

		while (RefSkeleton.GetRawParentIndex(CurrentJointId) != 0)
		{
			//Need to get parents by name
			const int32 ParentJointId = RefSkeleton.GetRawParentIndex(CurrentJointId);

			FTransform ParentTransform;
			Sequence->GetBoneTransform(ParentTransform, FSkeletonPoseBoneIndex(ParentJointId), NewTime, true);

			OutJointTransform = OutJointTransform * ParentTransform;
			CurrentJointId = ParentJointId;
		}
	}
	else
	{
		OutJointTransform = FTransform::Identity;
	}
}

void FMMPreProcessUtils::GetJointTransform_RootRelative(FTransform& OutTransform,
	UAnimSequence* AnimSequence, const TArray<FName>& BonesToRoot, const float Time)
{
	OutTransform = FTransform::Identity;

	if (!AnimSequence)
	{
		return;
	}

	for (const FName& BoneName : BonesToRoot)
	{
		FTransform BoneTransform;
		const int32 ConvertedBoneIndex = ConvertBoneNameToAnimBoneId(BoneName, AnimSequence);
		
		if (ConvertedBoneIndex == INDEX_NONE)
		{
			return;
		}

		AnimSequence->GetBoneTransform(BoneTransform, FSkeletonPoseBoneIndex(ConvertedBoneIndex), Time, true);

		OutTransform = OutTransform * BoneTransform;
	}

	const FTransform RootBoneTransform = AnimSequence->GetSkeleton()->GetReferenceSkeleton().GetRefBonePose()[0];
	OutTransform = OutTransform * RootBoneTransform;

	OutTransform.NormalizeRotation();
}

void FMMPreProcessUtils::GetJointTransform_RootRelative(FTransform& OutJointTransform, 
	const TArray<FBlendSampleData>& BlendSampleData, const TArray<FName>& BonesToRoot, const float Time)
{
	OutJointTransform = FTransform::Identity;

	if (BlendSampleData.Num() == 0 || BonesToRoot.Num() == 0)
	{
		return;
	}

	for (const FBlendSampleData& Sample : BlendSampleData)
	{
		if (!Sample.Animation)
		{
			continue;
		}
		
		const ScalarRegister VSampleWeight(Sample.GetClampedWeight());

		FTransform AnimJointTransform = FTransform::Identity;
		for (const FName& BoneName : BonesToRoot)
		{
			FTransform BoneTransform;
			const int32 ConvertedBoneIndex = ConvertBoneNameToAnimBoneId(BoneName, Sample.Animation);
			if (ConvertedBoneIndex == INDEX_NONE)
			{
				return;
			}

			Sample.Animation->GetBoneTransform(BoneTransform, FSkeletonPoseBoneIndex(ConvertedBoneIndex), Time, true);

			AnimJointTransform = AnimJointTransform * BoneTransform;
		}

		OutJointTransform.Accumulate(AnimJointTransform, VSampleWeight);
	}

	if(UAnimSequence* SourceAnim = BlendSampleData[0].Animation)
	{
		const FTransform RootBoneTransform = BlendSampleData[0].Animation->GetSkeleton()->GetReferenceSkeleton().GetRefBonePose()[0];
		OutJointTransform = OutJointTransform * RootBoneTransform;
	}

	OutJointTransform.NormalizeRotation();
}

void FMMPreProcessUtils::GetJointTransform_RootRelative(FTransform& OutTransform, 
	UAnimComposite* AnimComposite, const TArray<FName>& BonesToRoot, const float Time)
{
	OutTransform = FTransform::Identity;

	if (!AnimComposite)
	{
		return;
	}

	float CumDuration = 0.0f;
	UAnimSequence* Sequence = nullptr;
	float NewTime = Time;
	//for (const FAnimSegment& AnimSegment : AnimComposite->AnimationTrack.AnimSegments)
	for(int32 i = 0; i < AnimComposite->AnimationTrack.AnimSegments.Num(); ++i)
	{
		const FAnimSegment& AnimSegment = AnimComposite->AnimationTrack.AnimSegments[i];
		const float Length = AnimSegment.GetAnimReference()->GetPlayLength();

		if (Length + CumDuration > Time)
		{
			Sequence = Cast<UAnimSequence>(AnimSegment.GetAnimReference());
			break;
		}
		else
		{
			CumDuration += Length;
			NewTime -= Length;
		}
	}

	if (!Sequence)
	{
		return;
	}

	for (const FName& BoneName : BonesToRoot)
	{
		FTransform BoneTransform;
		const int32 ConvertedBoneIndex = ConvertBoneNameToAnimBoneId(BoneName, Sequence);
		if (ConvertedBoneIndex == INDEX_NONE)
		{
			return;
		}

		Sequence->GetBoneTransform(BoneTransform, FSkeletonPoseBoneIndex(ConvertedBoneIndex), NewTime, true);

		OutTransform = OutTransform * BoneTransform;
	}

	
	const FTransform RootBoneTransform = Sequence->GetSkeleton()->GetReferenceSkeleton().GetRefBonePose()[0];
	OutTransform = OutTransform * RootBoneTransform;

	OutTransform.NormalizeRotation();
}

void FMMPreProcessUtils::FindBonePathToRoot(const UAnimSequenceBase* AnimationSequenceBase, FName BoneName, TArray<FName>& BonePath)
{
	BonePath.Empty();
	if (AnimationSequenceBase)
	{
		BonePath.Add(BoneName);
		int32 BoneIndex = AnimationSequenceBase->GetSkeleton()->GetReferenceSkeleton().FindRawBoneIndex(BoneName);		
		if (BoneIndex != INDEX_NONE)
		{
			while (BoneIndex != INDEX_NONE)
			{
				const int32 ParentBoneIndex = AnimationSequenceBase->GetSkeleton()->GetReferenceSkeleton().GetRawParentIndex(BoneIndex);
				if (ParentBoneIndex != INDEX_NONE)
				{
					BonePath.Add(AnimationSequenceBase->GetSkeleton()->GetReferenceSkeleton().GetBoneName(ParentBoneIndex));
				}

				BoneIndex = ParentBoneIndex;
			}
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("Bone name %s not found in Skeleton %s"), *BoneName.ToString(), *AnimationSequenceBase->GetSkeleton()->GetName());
		}
	}
	else
	{
		UE_LOG(LogAnimation, Warning, TEXT("Invalid Animation Sequence supplied for FindBonePathToRoot"));
	}
}

FName FMMPreProcessUtils::FindMirrorBoneName(const USkeleton* InSkeleton, UMirrorDataTable* InMirrorDataTable, FName BoneName)
{
	if(!InSkeleton
		|| !InMirrorDataTable)
	{
		return BoneName;
	}
	
	const FReferenceSkeleton& RefSkeleton = InSkeleton->GetReferenceSkeleton();
	const FSkeletonPoseBoneIndex BoneIndex(RefSkeleton.FindBoneIndex(BoneName));
	const FSkeletonPoseBoneIndex MirrorBoneIndex = InMirrorDataTable->BoneToMirrorBoneIndex[BoneIndex];

	// In some cases the mirror bone index will be invalid, probably because the
	// bone is not in the mirror table. 
	if (RefSkeleton.IsValidIndex (MirrorBoneIndex.GetInt()))
	{
		return RefSkeleton.GetBoneName(MirrorBoneIndex.GetInt());
	}

	return BoneName;
}

