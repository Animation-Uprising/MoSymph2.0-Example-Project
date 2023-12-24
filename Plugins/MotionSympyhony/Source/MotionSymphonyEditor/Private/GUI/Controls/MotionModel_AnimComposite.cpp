//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MotionModel_AnimComposite.h"
#include "Animation/AnimComposite.h"
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
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"
#include "ScopedTransaction.h"
#include "Objects/MotionAnimObject.h"

#define LOCTEXT_NAMESPACE "FMotionModel_AnimComposite"

FMotionModel_AnimComposite::FMotionModel_AnimComposite(TObjectPtr<UMotionCompositeObject> InMotionComposite, UDebugSkelMeshComponent* InDebugSkelMesh)
	: FMotionModel(InMotionComposite, InDebugSkelMesh),
	AnimComposite(InMotionComposite->AnimComposite)
{
	SnapTypes.Add(FMotionModel::FSnapType::Frames.Type, FMotionModel::FSnapType::Frames);
	SnapTypes.Add(FMotionModel::FSnapType::Notifies.Type, FMotionModel::FSnapType::Notifies);

	FMotionModel_AnimComposite::UpdateRange();

	// Clear display flags
	for (bool& bElementNodeDisplayFlag : NotifiesTimingElementNodeDisplayFlags)
	{
		bElementNodeDisplayFlag = false;
	}

	//AnimComposite->RegisterOnAnimTrackCurvesChanged(UAnimSequenceBase::FOnAnimTrackCurvesChanged::CreateRaw(this, &FMotionModel_AnimSequenceBase::RefreshTracks));
	AnimComposite->RegisterOnNotifyChanged(UAnimComposite::FOnNotifyChanged::CreateRaw(this, &FMotionModel_AnimComposite::RefreshSnapTimes));

	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
}

FMotionModel_AnimComposite::~FMotionModel_AnimComposite()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}

	AnimComposite->UnregisterOnNotifyChanged(this);
	//AnimComposite->UnregisterOnAnimTrackCurvesChanged(this);
}

void FMotionModel_AnimComposite::RefreshTracks()
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

void FMotionModel_AnimComposite::Initialize()
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
		FExecuteAction::CreateSP(this, &FMotionModel_AnimComposite::EditSelectedCurves),
		FCanExecuteAction::CreateSP(this, &FMotionModel_AnimComposite::CanEditSelectedCurves));

	CommandList->MapAction(
		Commands.RemoveSelectedCurves,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimComposite::RemoveSelectedCurves));*/

	CommandList->MapAction(
		Commands.DisplayFrames,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimComposite::SetDisplayFormat, EFrameNumberDisplayFormats::Frames),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimComposite::IsDisplayFormatChecked, EFrameNumberDisplayFormats::Frames));

	CommandList->MapAction(
		Commands.DisplaySeconds,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimComposite::SetDisplayFormat, EFrameNumberDisplayFormats::Seconds),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimComposite::IsDisplayFormatChecked, EFrameNumberDisplayFormats::Seconds));

	CommandList->MapAction(
		Commands.DisplayPercentage,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimComposite::ToggleDisplayPercentage),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimComposite::IsDisplayPercentageChecked));

	CommandList->MapAction(
		Commands.DisplaySecondaryFormat,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimComposite::ToggleDisplaySecondary),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimComposite::IsDisplaySecondaryChecked));

	CommandList->MapAction(
		Commands.SnapToFrames,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimComposite::ToggleSnap, FMotionModel::FSnapType::Frames.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimComposite::IsSnapChecked, FMotionModel::FSnapType::Frames.Type),
		FIsActionButtonVisible::CreateSP(this, &FMotionModel_AnimComposite::IsSnapAvailable, FMotionModel::FSnapType::Frames.Type));

	CommandList->MapAction(
		Commands.SnapToNotifies,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimComposite::ToggleSnap, FMotionModel::FSnapType::Notifies.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimComposite::IsSnapChecked, FMotionModel::FSnapType::Notifies.Type),
		FIsActionButtonVisible::CreateSP(this, &FMotionModel_AnimComposite::IsSnapAvailable, FMotionModel::FSnapType::Notifies.Type));

	CommandList->MapAction(
		Commands.SnapToCompositeSegments,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimComposite::ToggleSnap, FMotionModel::FSnapType::CompositeSegment.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimComposite::IsSnapChecked, FMotionModel::FSnapType::CompositeSegment.Type),
		FIsActionButtonVisible::CreateSP(this, &FMotionModel_AnimComposite::IsSnapAvailable, FMotionModel::FSnapType::CompositeSegment.Type));

	CommandList->MapAction(
		Commands.SnapToMontageSections,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimComposite::ToggleSnap, FMotionModel::FSnapType::MontageSection.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimComposite::IsSnapChecked, FMotionModel::FSnapType::MontageSection.Type),
		FIsActionButtonVisible::CreateSP(this, &FMotionModel_AnimComposite::IsSnapAvailable, FMotionModel::FSnapType::MontageSection.Type));
}

void FMotionModel_AnimComposite::UpdateRange()
{
	const FAnimatedRange OldPlaybackRange = PlaybackRange;

	// update playback range
	PlaybackRange = FAnimatedRange(0.0, (double)AnimComposite->GetPlayLength());

	if (OldPlaybackRange != PlaybackRange)
	{
		// Update view/range if playback range changed
		SetViewRange(static_cast<TRange<double>>(PlaybackRange));
	}
}

double FMotionModel_AnimComposite::GetFrameRate() const
{
	if (AnimComposite && AnimComposite->AnimationTrack.AnimSegments.Num() > 0)
	{
		if(UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimComposite->AnimationTrack.AnimSegments[0].GetAnimReference()))
		{
			return (double)AnimSequence->GetSamplingFrameRate().AsDecimal();
		}
	}

	return 30.0f;
}

float FMotionModel_AnimComposite::GetPlayLength() const
{
	if (AnimComposite)
	{
		return AnimComposite->GetPlayLength();
	}

	return 0.0f;
}

bool FMotionModel_AnimComposite::IsNotifiesTimingElementDisplayEnabled(ETimingElementType::Type ElementType) const
{
	return NotifiesTimingElementNodeDisplayFlags[ElementType];
}

void FMotionModel_AnimComposite::ToggleNotifiesTimingElementDisplayEnabled(ETimingElementType::Type ElementType)
{
	NotifiesTimingElementNodeDisplayFlags[ElementType] = !NotifiesTimingElementNodeDisplayFlags[ElementType];
}

bool FMotionModel_AnimComposite::ClampToEndTime(float NewEndTime)
{
	float SequenceLength = AnimComposite->GetPlayLength();

	//if we had a valid sequence length before and our new end time is shorter
	//then we need to clamp.
	return (SequenceLength > 0.f && NewEndTime < SequenceLength);
}

void FMotionModel_AnimComposite::RefreshSnapTimes()
{
	SnapTimes.Reset();
	for (const FAnimNotifyEvent& Notify : AnimComposite->Notifies)
	{
		SnapTimes.Emplace(FSnapType::Notifies.Type, (double)Notify.GetTime());
		if (Notify.NotifyStateClass != nullptr)
		{
			SnapTimes.Emplace(FSnapType::Notifies.Type, (double)(Notify.GetTime() + Notify.GetDuration()));
		}
	}
}

void FMotionModel_AnimComposite::RefreshNotifyTracks()
{
	AnimComposite->InitializeNotifyTrack();

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

void FMotionModel_AnimComposite::SetDisplayFormat(EFrameNumberDisplayFormats InFormat)
{
	GetMutableDefault<UPersonaOptions>()->TimelineDisplayFormat = InFormat;
}

bool FMotionModel_AnimComposite::IsDisplayFormatChecked(EFrameNumberDisplayFormats InFormat) const
{
	return GetDefault<UPersonaOptions>()->TimelineDisplayFormat == InFormat;
}

void FMotionModel_AnimComposite::ToggleDisplayPercentage()
{
	GetMutableDefault<UPersonaOptions>()->bTimelineDisplayPercentage = !GetDefault<UPersonaOptions>()->bTimelineDisplayPercentage;
}

bool FMotionModel_AnimComposite::IsDisplayPercentageChecked() const
{
	return GetDefault<UPersonaOptions>()->bTimelineDisplayPercentage;
}

void FMotionModel_AnimComposite::ToggleDisplaySecondary()
{
	GetMutableDefault<UPersonaOptions>()->bTimelineDisplayFormatSecondary = !GetDefault<UPersonaOptions>()->bTimelineDisplayFormatSecondary;
}

bool FMotionModel_AnimComposite::IsDisplaySecondaryChecked() const
{
	return GetDefault<UPersonaOptions>()->bTimelineDisplayFormatSecondary;
}
void FMotionModel_AnimComposite::HandleUndoRedo()
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