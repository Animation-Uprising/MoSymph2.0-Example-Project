//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "SMotionTimelineTransportControls.h"
#include "EditorWidgetsModule.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimComposite.h"
#include "Animation/BlendSpace.h"
#include "AnimPreviewInstance.h"
#include "Modules/ModuleManager.h"

void SMotionTimelineTransportControls::Construct(const FArguments& InArgs, UDebugSkelMeshComponent* InDebugMesh,
	UAnimationAsset* InAnimAsset, EMotionAnimAssetType InAnimType)
{
	DebugSkelMesh = InDebugMesh;
	AnimAsset = InAnimAsset;
	AnimType = InAnimType;

	FrameRate = 30.0f;
	PlayLength = 0.0f;
	switch (AnimType)
	{
		case EMotionAnimAssetType::Sequence:
		{
			UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimAsset);
			PlayLength = AnimSequence ? AnimSequence->GetPlayLength() : 0.0f;
			FrameRate = AnimSequence ? AnimSequence->GetSamplingFrameRate().AsDecimal() : 30.0f;
		} break;
		case EMotionAnimAssetType::BlendSpace:
		{
			UBlendSpace* BlendSpace = Cast<UBlendSpace>(AnimAsset);
			if (BlendSpace)
			{
				PlayLength = BlendSpace->AnimLength;

				TArray<UAnimationAsset*> BSSequences;
				BlendSpace->GetAllAnimationSequencesReferred(BSSequences, false);

				if (BSSequences.Num() > 0)
				{
					UAnimSequence* AnimSequence = Cast<UAnimSequence>(BSSequences[0]);
					FrameRate = AnimSequence ? AnimSequence->GetSamplingFrameRate().AsDecimal() : FrameRate;
				}
			}

		} break;
		case EMotionAnimAssetType::Composite:
		{
			UAnimComposite* AnimComposite = Cast<UAnimComposite>(AnimAsset);
			if (AnimComposite && AnimComposite->AnimationTrack.AnimSegments.Num() > 0)
			{
				PlayLength = AnimComposite->GetPlayLength();
				UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimComposite->AnimationTrack.AnimSegments[0].GetAnimReference());
				FrameRate = AnimSequence ? static_cast<float>(AnimSequence->GetSamplingFrameRate().AsDecimal()) : FrameRate;			
			}
		} break;
	}

	//check(AnimSequenceBase);

	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::LoadModuleChecked<FEditorWidgetsModule>("EditorWidgets");

	FTransportControlArgs TransportControlArgs;
	TransportControlArgs.OnForwardPlay = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_Forward);
	TransportControlArgs.OnRecord = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_Record);
	TransportControlArgs.OnBackwardPlay = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_Backward);
	TransportControlArgs.OnForwardStep = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_Forward_Step);
	TransportControlArgs.OnBackwardStep = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_Backward_Step);
	TransportControlArgs.OnForwardEnd = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_Forward_End);
	TransportControlArgs.OnBackwardEnd = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_Backward_End);
	TransportControlArgs.OnToggleLooping = FOnClicked::CreateSP(this, &SMotionTimelineTransportControls::OnClick_ToggleLoop);
	TransportControlArgs.OnGetLooping = FOnGetLooping::CreateSP(this, &SMotionTimelineTransportControls::IsLoopStatusOn);
	TransportControlArgs.OnGetPlaybackMode = FOnGetPlaybackMode::CreateSP(this, &SMotionTimelineTransportControls::GetPlaybackMode);
	TransportControlArgs.OnGetRecording = FOnGetRecording::CreateSP(this, &SMotionTimelineTransportControls::IsRecording);

	ChildSlot
	[
		EditorWidgetsModule.CreateTransportControl(TransportControlArgs)
	];


}

UAnimSingleNodeInstance* SMotionTimelineTransportControls::GetPreviewInstance() const
{
	if (DebugSkelMesh && DebugSkelMesh->IsPreviewOn())
	{
		return DebugSkelMesh->PreviewInstance;
	}

	return nullptr;
}

FReply SMotionTimelineTransportControls::OnClick_Forward_Step()
{
	if(!DebugSkelMesh)
		return FReply::Handled();

	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		bool bShouldStepCloth = FMath::Abs(PreviewInstance->GetLength() - PreviewInstance->GetCurrentTime()) > SMALL_NUMBER;

		PreviewInstance->SetPlaying(false);
		PreviewInstance->StepForward();

		if (DebugSkelMesh && bShouldStepCloth)
		{
			DebugSkelMesh->bPerformSingleClothingTick = true;
		}
	}
	else if (DebugSkelMesh)
	{


		// Advance a single frame, leaving it paused afterwards
		DebugSkelMesh->GlobalAnimRateScale = 1.0f;
		DebugSkelMesh->TickAnimation(1.0f / FrameRate, false);
		DebugSkelMesh->GlobalAnimRateScale = 0.0f;
	}

	return FReply::Handled();
}

FReply SMotionTimelineTransportControls::OnClick_Forward_End()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		PreviewInstance->SetPlaying(false);
		PreviewInstance->SetPosition(PreviewInstance->GetLength(), false);
	}

	return FReply::Handled();
}

FReply SMotionTimelineTransportControls::OnClick_Backward_Step()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		bool bShouldStepCloth = PreviewInstance->GetCurrentTime() > SMALL_NUMBER;

		PreviewInstance->SetPlaying(false);
		PreviewInstance->StepBackward();

		if (DebugSkelMesh && bShouldStepCloth)
		{
			DebugSkelMesh->bPerformSingleClothingTick = true;
		}
	}
	return FReply::Handled();
}

FReply SMotionTimelineTransportControls::OnClick_Backward_End()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		PreviewInstance->SetPlaying(false);
		PreviewInstance->SetPosition(0.f, false);
	}
	return FReply::Handled();
}

FReply SMotionTimelineTransportControls::OnClick_Forward()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();

	if (PreviewInstance)
	{
		bool bIsReverse = PreviewInstance->IsReverse();
		bool bIsPlaying = PreviewInstance->IsPlaying();
		// if current bIsReverse and bIsPlaying, we'd like to just turn off reverse
		if (bIsReverse && bIsPlaying)
		{
			PreviewInstance->SetReverse(false);
		}
		// already playing, simply pause
		else if (bIsPlaying)
		{
			PreviewInstance->SetPlaying(false);

			if (DebugSkelMesh && DebugSkelMesh->bPauseClothingSimulationWithAnim)
			{
				DebugSkelMesh->SuspendClothingSimulation();
			}
		}
		// if not playing, play forward
		else
		{
			//if we're at the end of the animation, jump back to the beginning before playing
			if (PreviewInstance->GetCurrentTime() >= PlayLength)
			{
				PreviewInstance->SetPosition(0.0f, false);
			}

			PreviewInstance->SetReverse(false);
			PreviewInstance->SetPlaying(true);

			if (DebugSkelMesh && DebugSkelMesh->bPauseClothingSimulationWithAnim)
			{
				DebugSkelMesh->ResumeClothingSimulation();
			}
		}
	}
	else if (DebugSkelMesh)
	{
		DebugSkelMesh->GlobalAnimRateScale = (DebugSkelMesh->GlobalAnimRateScale > 0.0f) ? 0.0f : 1.0f;
	}

	return FReply::Handled();
}

FReply SMotionTimelineTransportControls::OnClick_Backward()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		bool bIsReverse = PreviewInstance->IsReverse();
		bool bIsPlaying = PreviewInstance->IsPlaying();
		// if currently playing forward, just simply turn on reverse
		if (!bIsReverse && bIsPlaying)
		{
			PreviewInstance->SetReverse(true);
		}
		else if (bIsPlaying)
		{
			PreviewInstance->SetPlaying(false);
		}
		else
		{
			//if we're at the beginning of the animation, jump back to the end before playing
			if (PreviewInstance->GetCurrentTime() <= 0.0f)
			{
				PreviewInstance->SetPosition(PlayLength, false);
			}

			PreviewInstance->SetPlaying(true);
			PreviewInstance->SetReverse(true);
		}
	}
	return FReply::Handled();
}

FReply SMotionTimelineTransportControls::OnClick_ToggleLoop()
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	if (PreviewInstance)
	{
		bool bIsLooping = PreviewInstance->IsLooping();
		PreviewInstance->SetLooping(!bIsLooping);
	}
	return FReply::Handled();
}

FReply SMotionTimelineTransportControls::OnClick_Record()
{
	//StaticCastSharedRef<FAnimationEditorPreviewScene>(GetPreviewScene())->RecordAnimation();

	return FReply::Handled();
}

bool SMotionTimelineTransportControls::IsLoopStatusOn() const
{
	UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance();
	return (PreviewInstance && PreviewInstance->IsLooping());
}

EPlaybackMode::Type SMotionTimelineTransportControls::GetPlaybackMode() const
{
	if (UAnimSingleNodeInstance* PreviewInstance = GetPreviewInstance())
	{
		if (PreviewInstance->IsPlaying())
		{
			return PreviewInstance->IsReverse() ? EPlaybackMode::PlayingReverse : EPlaybackMode::PlayingForward;
		}
		return EPlaybackMode::Stopped;
	}
	else if (DebugSkelMesh)
	{
		return (DebugSkelMesh->GlobalAnimRateScale > 0.0f) ? EPlaybackMode::PlayingForward : EPlaybackMode::Stopped;
	}

	return EPlaybackMode::Stopped;
}

bool SMotionTimelineTransportControls::IsRecording() const
{
	//return StaticCastSharedRef<FAnimationEditorPreviewScene>(GetPreviewScene())->IsRecording();

	return false;
}