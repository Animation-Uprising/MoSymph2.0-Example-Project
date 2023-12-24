//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MotionModel_BlendSpace.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"
#include "Controls/MotionTimelineTrack.h"
#include "MotionTimelineTrack_Tags.h"
#include "MotionTimelineTrack_TagsPanel.h"
#include "MotionSequenceTimelineCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "IAnimationEditor.h"
#include "Preferences/PersonaOptions.h"
#include "FrameNumberDisplayFormat.h"
#include "Framework/Commands/GenericCommands.h"
#if ENGINE_MINOR_VERSION > 2
#include "AnimatedRange.h"
#endif
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"
#include "ScopedTransaction.h"
#include "Objects/MotionAnimObject.h"

#define LOCTEXT_NAMESPACE "FMotionModel_BlendSpace"

FMotionModel_BlendSpace::FMotionModel_BlendSpace(TObjectPtr<UMotionBlendSpaceObject> InMotionBlendSpace, UDebugSkelMeshComponent* InDebugSkelMesh)
	: FMotionModel(InMotionBlendSpace, InDebugSkelMesh),
	BlendSpace(InMotionBlendSpace->BlendSpace)
{
	SnapTypes.Add(FMotionModel::FSnapType::Frames.Type, FMotionModel::FSnapType::Frames);
	SnapTypes.Add(FMotionModel::FSnapType::Notifies.Type, FMotionModel::FSnapType::Notifies);

	UpdateRange();

	// Clear display flags
	for (bool& bElementNodeDisplayFlag : NotifiesTimingElementNodeDisplayFlags)
	{
		bElementNodeDisplayFlag = false;
	}

	//AnimComposite->RegisterOnAnimTrackCurvesChanged(UAnimSequenceBase::FOnAnimTrackCurvesChanged::CreateRaw(this, &FMotionModel_AnimSequenceBase::RefreshTracks));
	//AnimComposite->RegisterOnNotifyChanged(UAnimComposite::FOnNotifyChanged::CreateRaw(this, &FMotionModel_AnimComposite::RefreshSnapTimes));

	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
}

FMotionModel_BlendSpace::~FMotionModel_BlendSpace()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}

	//AnimComposite->UnregisterOnNotifyChanged(this);
	//AnimComposite->UnregisterOnAnimTrackCurvesChanged(this);
}

void FMotionModel_BlendSpace::RefreshTracks()
{
	ClearTrackSelection();

	// Clear all tracks
	RootTracks.Empty();

	//Add Notifies
	RefreshNotifyTracks();

	//Add Curves
	//RefreshCurveTracks();

	//Snaps
	RefreshSnapTimes();

	OnTracksChangedDelegate.Broadcast();

	UpdateRange();
}

void FMotionModel_BlendSpace::Initialize()
{
	if (!WeakCommandList.IsValid())
		return;

	TSharedRef<FUICommandList> CommandList = WeakCommandList.Pin().ToSharedRef();

	const FMotionSequenceTimelineCommands& Commands = FMotionSequenceTimelineCommands::Get();

	CommandList->MapAction(
		FGenericCommands::Get().Rename,
		FExecuteAction::CreateLambda([this]
			{
				SelectedTracks.Array()[0]->RequestRename();
			}),
		FCanExecuteAction::CreateLambda([this]
			{
				return (SelectedTracks.Num() > 0) && (SelectedTracks.Array()[0]->CanRename());
			})
		);

	/*CommandList->MapAction(
		Commands.EditSelectedCurves,
		FExecuteAction::CreateSP(this, &FMotionModel_BlendSpace::EditSelectedCurves),
		FCanExecuteAction::CreateSP(this, &FMotionModel_BlendSpace::CanEditSelectedCurves));

	CommandList->MapAction(
		Commands.RemoveSelectedCurves,
		FExecuteAction::CreateSP(this, &FMotionModel_BlendSpace::RemoveSelectedCurves));*/

	CommandList->MapAction(
		Commands.DisplayFrames,
		FExecuteAction::CreateSP(this, &FMotionModel_BlendSpace::SetDisplayFormat, EFrameNumberDisplayFormats::Frames),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_BlendSpace::IsDisplayFormatChecked, EFrameNumberDisplayFormats::Frames));

	CommandList->MapAction(
		Commands.DisplaySeconds,
		FExecuteAction::CreateSP(this, &FMotionModel_BlendSpace::SetDisplayFormat, EFrameNumberDisplayFormats::Seconds),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_BlendSpace::IsDisplayFormatChecked, EFrameNumberDisplayFormats::Seconds));

	CommandList->MapAction(
		Commands.DisplayPercentage,
		FExecuteAction::CreateSP(this, &FMotionModel_BlendSpace::ToggleDisplayPercentage),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_BlendSpace::IsDisplayPercentageChecked));

	CommandList->MapAction(
		Commands.DisplaySecondaryFormat,
		FExecuteAction::CreateSP(this, &FMotionModel_BlendSpace::ToggleDisplaySecondary),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_BlendSpace::IsDisplaySecondaryChecked));

	CommandList->MapAction(
		Commands.SnapToFrames,
		FExecuteAction::CreateSP(this, &FMotionModel_BlendSpace::ToggleSnap, FMotionModel::FSnapType::Frames.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_BlendSpace::IsSnapChecked, FMotionModel::FSnapType::Frames.Type),
		FIsActionButtonVisible::CreateSP(this, &FMotionModel_BlendSpace::IsSnapAvailable, FMotionModel::FSnapType::Frames.Type));

	CommandList->MapAction(
		Commands.SnapToNotifies,
		FExecuteAction::CreateSP(this, &FMotionModel_BlendSpace::ToggleSnap, FMotionModel::FSnapType::Notifies.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_BlendSpace::IsSnapChecked, FMotionModel::FSnapType::Notifies.Type),
		FIsActionButtonVisible::CreateSP(this, &FMotionModel_BlendSpace::IsSnapAvailable, FMotionModel::FSnapType::Notifies.Type));

	CommandList->MapAction(
		Commands.SnapToCompositeSegments,
		FExecuteAction::CreateSP(this, &FMotionModel_BlendSpace::ToggleSnap, FMotionModel::FSnapType::CompositeSegment.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_BlendSpace::IsSnapChecked, FMotionModel::FSnapType::CompositeSegment.Type),
		FIsActionButtonVisible::CreateSP(this, &FMotionModel_BlendSpace::IsSnapAvailable, FMotionModel::FSnapType::CompositeSegment.Type));

	CommandList->MapAction(
		Commands.SnapToMontageSections,
		FExecuteAction::CreateSP(this, &FMotionModel_BlendSpace::ToggleSnap, FMotionModel::FSnapType::MontageSection.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_BlendSpace::IsSnapChecked, FMotionModel::FSnapType::MontageSection.Type),
		FIsActionButtonVisible::CreateSP(this, &FMotionModel_BlendSpace::IsSnapAvailable, FMotionModel::FSnapType::MontageSection.Type));
}

void FMotionModel_BlendSpace::UpdateRange()
{
	FAnimatedRange OldPlaybackRange = PlaybackRange;

	// update playback range
	PlaybackRange = FAnimatedRange(0.0, (double)BlendSpace->AnimLength);

	if (OldPlaybackRange != PlaybackRange)
	{
		// Update view/range if playback range changed
		SetViewRange(PlaybackRange);
	}
}

double FMotionModel_BlendSpace::GetFrameRate() const
{
	if (BlendSpace)
	{
		TArray<UAnimationAsset*> BlendSpaceAnims;
		BlendSpace->GetAllAnimationSequencesReferred(BlendSpaceAnims);

		if (BlendSpaceAnims.Num() > 0)
		{
			UAnimSequence* AnimSequence = Cast<UAnimSequence>(BlendSpaceAnims[0]);

			if (AnimSequence)
			{
				return (double)AnimSequence->GetSamplingFrameRate().AsDecimal();
			}
		}
	}

	return 30.0f;
}

float FMotionModel_BlendSpace::GetPlayLength() const
{
	if (BlendSpace)
	{
		return BlendSpace->AnimLength;
	}

	return 0.0f;
}

bool FMotionModel_BlendSpace::IsNotifiesTimingElementDisplayEnabled(ETimingElementType::Type ElementType) const
{
	return NotifiesTimingElementNodeDisplayFlags[ElementType];
}

void FMotionModel_BlendSpace::ToggleNotifiesTimingElementDisplayEnabled(ETimingElementType::Type ElementType)
{
	NotifiesTimingElementNodeDisplayFlags[ElementType] = !NotifiesTimingElementNodeDisplayFlags[ElementType];
}

bool FMotionModel_BlendSpace::ClampToEndTime(float NewEndTime)
{
	//if we had a valid sequence length before and our new end time is shorter
	//then we need to clamp.
	return (BlendSpace->AnimLength > 0.0f && NewEndTime < BlendSpace->AnimLength);
}

void FMotionModel_BlendSpace::RefreshSnapTimes()
{
	/*SnapTimes.Reset();
	for (const FAnimNotifyEvent& Notify : AnimComposite->Notifies)
	{
		SnapTimes.Emplace(FSnapType::Notifies.Type, (double)Notify.GetTime());
		if (Notify.NotifyStateClass != nullptr)
		{
			SnapTimes.Emplace(FSnapType::Notifies.Type, (double)(Notify.GetTime() + Notify.GetDuration()));
		}
	}*/
}

void FMotionModel_BlendSpace::RefreshNotifyTracks()
{
	//BlendSpace->InitializeNotifyTrack();

	if (!TagRoot.IsValid())
	{
		// Add a root track for notifies & then the main 'panel' legacy track
		TagRoot = MakeShared<FMotionTimelineTrack_Tags>(SharedThis(this));
	}

	TagRoot->ClearChildren();
	RootTracks.Add(TagRoot.ToSharedRef());

	if (!TagPanel.IsValid())
	{
		TagPanel = MakeShared<FMotionTimelineTrack_TagsPanel>(SharedThis(this));
		TagRoot->SetMotionTagPanel(TagPanel.ToSharedRef());
	}

	TagRoot->AddChild(TagPanel.ToSharedRef());
}

void FMotionModel_BlendSpace::SetDisplayFormat(EFrameNumberDisplayFormats InFormat)
{
	GetMutableDefault<UPersonaOptions>()->TimelineDisplayFormat = InFormat;
}

bool FMotionModel_BlendSpace::IsDisplayFormatChecked(EFrameNumberDisplayFormats InFormat) const
{
	return GetDefault<UPersonaOptions>()->TimelineDisplayFormat == InFormat;
}

void FMotionModel_BlendSpace::ToggleDisplayPercentage()
{
	GetMutableDefault<UPersonaOptions>()->bTimelineDisplayPercentage = !GetDefault<UPersonaOptions>()->bTimelineDisplayPercentage;
}

bool FMotionModel_BlendSpace::IsDisplayPercentageChecked() const
{
	return GetDefault<UPersonaOptions>()->bTimelineDisplayPercentage;
}

void FMotionModel_BlendSpace::ToggleDisplaySecondary()
{
	GetMutableDefault<UPersonaOptions>()->bTimelineDisplayFormatSecondary = !GetDefault<UPersonaOptions>()->bTimelineDisplayFormatSecondary;
}

bool FMotionModel_BlendSpace::IsDisplaySecondaryChecked() const
{
	return GetDefault<UPersonaOptions>()->bTimelineDisplayFormatSecondary;
}

void FMotionModel_BlendSpace::HandleUndoRedo()
{
	// Close any curves that are no longer editable
	/*for (FFloatCurve& FloatCurve : AnimSequenceBase->RawCurveData.FloatCurves)
	{
		if (FloatCurve.GetCurveTypeFlag(AACF_Metadata))
		{
			IAnimationEditor::FCurveEditInfo CurveEditInfo(FloatCurve.Name, ERawCurveTrackTypes::RCT_Float, 0);
			OnStopEditingCurves.ExecuteIfBound(TArray<IAnimationEditor::FCurveEditInfo>({ CurveEditInfo }));
		}
	}*/
}

#undef LOCTEXT_NAMESPACE