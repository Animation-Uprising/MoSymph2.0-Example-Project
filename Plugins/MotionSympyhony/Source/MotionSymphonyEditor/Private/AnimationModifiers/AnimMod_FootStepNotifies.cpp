//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "AnimationModifiers/AnimMod_FootStepNotifies.h"
#include "Animation/AnimSequence.h"
#include "Objects/AnimNotifies/AnimNotify_MSFootLockSingle.h"
#include "Objects/AnimNotifies/AnimNotify_MSFootLockTimer.h"
#include "Utility/MMPreProcessUtils.h"

void UAnimMod_FootStepNotifies::OnApply_Implementation(UAnimSequence* AnimationSequence)
{
	if(!AnimationSequence)
	{
		return;
	}

	for(const FMSFootLockLimb& Limb : ToeLimbs)
	{

		//First try find the track if it already exists
		int32 TrackIndex = -1;
		FName TrackName = FName(TEXT("FootStep_") + Limb.ToeName.ToString());
		for(int32 i = 0; i < AnimationSequence->AnimNotifyTracks.Num(); ++i)
		{
			const FAnimNotifyTrack& Track = AnimationSequence->AnimNotifyTracks[i];
			if(TrackName == Track.TrackName)
			{
				TrackIndex = i;
				break;
			}
		}

		//If the track wasn't found, create it
		if(TrackIndex == -1)
		{
			AnimationSequence->AnimNotifyTracks.Emplace(TrackName, FLinearColor::Green);
		}
		
		AnalyseToe(Limb, AnimationSequence, TrackIndex);
	}
}

void UAnimMod_FootStepNotifies::OnRevert_Implementation(UAnimSequence* AnimationSequence)
{
	if(!AnimationSequence)
	{
		return;
	}

	for(const FMSFootLockLimb& Limb : ToeLimbs)
	{
		FName TrackName = FName(TEXT("FootStep_") + Limb.ToeName.ToString());
		for(int32 TrackIndex = 0; TrackIndex < AnimationSequence->AnimNotifyTracks.Num(); ++TrackIndex)
		{
			const FAnimNotifyTrack& Track = AnimationSequence->AnimNotifyTracks[TrackIndex];
			if(TrackName == Track.TrackName)
			{
				for(int32 NotifyIndex = 0; NotifyIndex < AnimationSequence->Notifies.Num(); ++NotifyIndex)
				{
					const FAnimNotifyEvent& Notify = AnimationSequence->Notifies[NotifyIndex];
					if(Notify.TrackIndex == TrackIndex)
					{
						AnimationSequence->Notifies.RemoveAt(NotifyIndex);
						--NotifyIndex;
					}
				}
				
				AnimationSequence->AnimNotifyTracks.RemoveAt(TrackIndex);
				--TrackIndex;
			}
		}
	}
}

void UAnimMod_FootStepNotifies::AnalyseToe(const FMSFootLockLimb& Limb, UAnimSequence* AnimationSequence, const int32 TrackIndex)
{
	if(!AnimationSequence)
	{
		return;
	}
	
	FFloatCurve TempFootSpeedCurve;
	FFloatCurve TempFootHeightCurve;

	const int32 NumFrames = AnimationSequence->GetNumberOfSampledKeys();
	const float FrameRate = 1.0f / AnimationSequence->GetSamplingFrameRate().AsDecimal();

	TArray<FName> BonesToRoot;
	FMMPreProcessUtils::FindBonePathToRoot(AnimationSequence, Limb.ToeName, BonesToRoot);
	
	//Pass 1: Create the foot speed curves (world space)
	for(int32 Frame = 0; Frame < NumFrames; ++Frame)
	{
		const float FrameTime = Frame * FrameRate;
		const int32 LastFrame = FMath::Clamp(Frame - 1, 0, NumFrames);
		const int32 NextFrame = FMath::Clamp(Frame + 1, 0, NumFrames);
		
		//Get Foot Speed
		FTransform LastFrameTransform;
		FTransform NextFrameTransform;
		FMMPreProcessUtils::GetJointTransform_RootRelative(LastFrameTransform, AnimationSequence, BonesToRoot, LastFrame * FrameRate);
		FMMPreProcessUtils::GetJointTransform_RootRelative(NextFrameTransform, AnimationSequence, BonesToRoot, NextFrame * FrameRate);
		
		const float FootSpeed = (NextFrameTransform.GetLocation() - LastFrameTransform.GetLocation()).Length() * FrameRate / (NextFrame - LastFrame);
		TempFootSpeedCurve.UpdateOrAddKey(FootSpeed, FrameTime);
	}

	if(BonesToRoot.Num() > 0)
	{
		BonesToRoot.RemoveAt(BonesToRoot.Num()-1);
	}

	//Create the foot height (above root) curve
	for(int32 Frame = 0; Frame < NumFrames; ++Frame)
	{
		//Get Foot Height
		const float FrameTime = Frame * FrameRate;
		
		FTransform ThisFrameTransform;
		FMMPreProcessUtils::GetJointTransform_RootRelative(ThisFrameTransform, AnimationSequence, BonesToRoot, FrameTime);
		TempFootHeightCurve.UpdateOrAddKey(ThisFrameTransform.GetLocation().Z, FrameTime);
	}

	//Pass 3: Analyse the curves and add notifies
	bool bLockedNow = false;
	float LastLockStart = 0.0f;
	for(int32 Key = 0; Key < TempFootSpeedCurve.FloatCurve.GetNumKeys(); ++Key)
	{
		const float Time = Key * FrameRate;
		const float Speed = TempFootSpeedCurve.Evaluate(Time);
		const float Height = TempFootHeightCurve.Evaluate(Time);
		
		if(bLockedNow) //Check criteria to see if it should be unlocked
		{
			if(Speed > SpeedThreshold + SpeedOverlap
				|| Height > HeightThreshold + HeightOverlap)
			{
				float GroundingTime = Time - LastLockStart;

				if(GroundingTime > MinFootStepDuration)
				{
					AddLockNotify(LastLockStart, GroundingTime, Limb, AnimationSequence, TrackIndex);

					if(bAddEndNotify)
					{
						AddUnlockNotify(Time, Limb, AnimationSequence, TrackIndex);
					}
				}

				bLockedNow = false;
			}
		}
		else //Check criteria to see if it should be locked
		{
			if(Speed < SpeedThreshold - SpeedOverlap
				&& Height < HeightThreshold - HeightOverlap)
			{
				bLockedNow = true;
				LastLockStart = Time;
			}
		}
	}

	if(bLockedNow)
	{
		float GroundingTime = AnimationSequence->GetPlayLength() - LastLockStart;

		AddLockNotify(LastLockStart, GroundingTime, Limb, AnimationSequence, TrackIndex);
	}
}

void UAnimMod_FootStepNotifies::AddLockNotify(const float Time, const float GroundingTime,
                                              const FMSFootLockLimb& Limb, UAnimSequence* AnimationSequence, const int32 TrackIndex)
{
	if(!AnimationSequence)
	{
		return;
	}

	UAnimNotify_MSFootLockTimer* NotifyClass = NewObject<UAnimNotify_MSFootLockTimer>(AnimationSequence,
						UAnimNotify_MSFootLockTimer::StaticClass(), NAME_None, RF_Transactional);

	const int32 NotifyIndex = AnimationSequence->Notifies.Add(FAnimNotifyEvent());
	NotifyClass->FootLockId = Limb.FootId;
	NotifyClass->GroundingTime = GroundingTime;
					
	FAnimNotifyEvent& NewNotifyEvent = AnimationSequence->Notifies[NotifyIndex];
	NewNotifyEvent.TrackIndex = TrackIndex;
	NewNotifyEvent.Notify = NotifyClass;
	NewNotifyEvent.Link(AnimationSequence, Time);
}

void UAnimMod_FootStepNotifies::AddUnlockNotify(const float Time, const FMSFootLockLimb& Limb,
                                                UAnimSequence* AnimationSequence, const int32 TrackIndex)
{
	if(!AnimationSequence)
	{
		return;
	}

	UAnimNotify_MSFootLockSingle* NotifyClass = NewObject<UAnimNotify_MSFootLockSingle>(AnimationSequence,
		UAnimNotify_MSFootLockSingle::StaticClass(), NAME_None, RF_Transactional);

	const int32 NotifyIndex = AnimationSequence->Notifies.Add(FAnimNotifyEvent());
	NotifyClass->bSetLocked = false;
	NotifyClass->FootLockId = Limb.FootId;
	
	
	FAnimNotifyEvent& NewNotifyEvent = AnimationSequence->Notifies[NotifyIndex];
	NewNotifyEvent.TrackIndex = TrackIndex;
	NewNotifyEvent.Notify = NotifyClass;
	NewNotifyEvent.Link(AnimationSequence, Time);
}