//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MotionModel_AnimSequenceBase.h"
#include "Animation/AnimSequence.h"
#include "Controls/MotionTimelineTrack.h"
#include "MotionTimelineTrack_Tags.h"
#include "MotionTimelineTrack_TagsPanel.h"
//#include "AnimTimelineTrack_Curves.h"
//#include "AnimTimelineTrack_Curve.h"
//#include "AnimTimelineTrack_FloatCurve.h"
//#include "AnimTimelineTrack_VectorCurve.h"
//#include "AnimTimelineTrack_TransformCurve.h"
#include "MotionSequenceTimelineCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "IAnimationEditor.h"
#include "Preferences/PersonaOptions.h"
#include "FrameNumberDisplayFormat.h"
#include "Framework/Commands/GenericCommands.h"
//#include "IPersonaPreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"
#include "ScopedTransaction.h"
#include "Objects/MotionAnimObject.h"

#define LOCTEXT_NAMESPACE "FMotionModel_AnimSequence"

FMotionModel_AnimSequenceBase::FMotionModel_AnimSequenceBase(TObjectPtr<UMotionSequenceObject> InMotionAnim, UDebugSkelMeshComponent* InDebugSkelMesh)
	: FMotionModel(InMotionAnim, InDebugSkelMesh)
	, AnimSequenceBase(InMotionAnim->Sequence)
{
	SnapTypes.Add(FMotionModel::FSnapType::Frames.Type, FMotionModel::FSnapType::Frames);
	SnapTypes.Add(FMotionModel::FSnapType::Notifies.Type, FMotionModel::FSnapType::Notifies);

	UpdateRange();

	// Clear display flags
	for (bool& bElementNodeDisplayFlag : NotifiesTimingElementNodeDisplayFlags)
	{
		bElementNodeDisplayFlag = false;
	}

	//AnimSequenceBase->RegisterOnAnimTrackCurvesChanged(UAnimSequenceBase::FOnAnimTrackCurvesChanged::CreateRaw(this, &FMotionModel_AnimSequenceBase::RefreshTracks));
	AnimSequenceBase->RegisterOnNotifyChanged(UAnimSequenceBase::FOnNotifyChanged::CreateRaw(this, &FMotionModel_AnimSequenceBase::RefreshSnapTimes));

	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
}

FMotionModel_AnimSequenceBase::~FMotionModel_AnimSequenceBase()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}

	AnimSequenceBase->UnregisterOnNotifyChanged(this);
	//AnimSequenceBase->UnregisterOnAnimTrackCurvesChanged(this);
}

void FMotionModel_AnimSequenceBase::RefreshTracks()
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

void FMotionModel_AnimSequenceBase::Initialize()
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
		FExecuteAction::CreateSP(this, &FMotionModel_AnimSequenceBase::EditSelectedCurves),
		FCanExecuteAction::CreateSP(this, &FMotionModel_AnimSequenceBase::CanEditSelectedCurves));

	CommandList->MapAction(
		Commands.RemoveSelectedCurves,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimSequenceBase::RemoveSelectedCurves));*/

	CommandList->MapAction(
		Commands.DisplayFrames,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimSequenceBase::SetDisplayFormat, EFrameNumberDisplayFormats::Frames),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimSequenceBase::IsDisplayFormatChecked, EFrameNumberDisplayFormats::Frames));

	CommandList->MapAction(
		Commands.DisplaySeconds,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimSequenceBase::SetDisplayFormat, EFrameNumberDisplayFormats::Seconds),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimSequenceBase::IsDisplayFormatChecked, EFrameNumberDisplayFormats::Seconds));

	CommandList->MapAction(
		Commands.DisplayPercentage,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimSequenceBase::ToggleDisplayPercentage),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimSequenceBase::IsDisplayPercentageChecked));

	CommandList->MapAction(
		Commands.DisplaySecondaryFormat,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimSequenceBase::ToggleDisplaySecondary),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimSequenceBase::IsDisplaySecondaryChecked));

	CommandList->MapAction(
		Commands.SnapToFrames,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimSequenceBase::ToggleSnap, FMotionModel::FSnapType::Frames.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimSequenceBase::IsSnapChecked, FMotionModel::FSnapType::Frames.Type),
		FIsActionButtonVisible::CreateSP(this, &FMotionModel_AnimSequenceBase::IsSnapAvailable, FMotionModel::FSnapType::Frames.Type));

	CommandList->MapAction(
		Commands.SnapToNotifies,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimSequenceBase::ToggleSnap, FMotionModel::FSnapType::Notifies.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimSequenceBase::IsSnapChecked, FMotionModel::FSnapType::Notifies.Type),
		FIsActionButtonVisible::CreateSP(this, &FMotionModel_AnimSequenceBase::IsSnapAvailable, FMotionModel::FSnapType::Notifies.Type));

	CommandList->MapAction(
		Commands.SnapToCompositeSegments,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimSequenceBase::ToggleSnap, FMotionModel::FSnapType::CompositeSegment.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimSequenceBase::IsSnapChecked, FMotionModel::FSnapType::CompositeSegment.Type),
		FIsActionButtonVisible::CreateSP(this, &FMotionModel_AnimSequenceBase::IsSnapAvailable, FMotionModel::FSnapType::CompositeSegment.Type));

	CommandList->MapAction(
		Commands.SnapToMontageSections,
		FExecuteAction::CreateSP(this, &FMotionModel_AnimSequenceBase::ToggleSnap, FMotionModel::FSnapType::MontageSection.Type),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FMotionModel_AnimSequenceBase::IsSnapChecked, FMotionModel::FSnapType::MontageSection.Type),
		FIsActionButtonVisible::CreateSP(this, &FMotionModel_AnimSequenceBase::IsSnapAvailable, FMotionModel::FSnapType::MontageSection.Type));
}

void FMotionModel_AnimSequenceBase::UpdateRange()
{
	FAnimatedRange OldPlaybackRange = PlaybackRange;

	// update playback range
	PlaybackRange = FAnimatedRange(0.0, (double)AnimSequenceBase->GetPlayLength());

	if (OldPlaybackRange != PlaybackRange)
	{
		// Update view/range if playback range changed
		SetViewRange(PlaybackRange);
	}
}

bool FMotionModel_AnimSequenceBase::IsNotifiesTimingElementDisplayEnabled(ETimingElementType::Type ElementType) const
{
	return NotifiesTimingElementNodeDisplayFlags[ElementType];
}

void FMotionModel_AnimSequenceBase::ToggleNotifiesTimingElementDisplayEnabled(ETimingElementType::Type ElementType)
{
	NotifiesTimingElementNodeDisplayFlags[ElementType] = !NotifiesTimingElementNodeDisplayFlags[ElementType];
}

bool FMotionModel_AnimSequenceBase::ClampToEndTime(float NewEndTime)
{
	float SequenceLength = AnimSequenceBase->GetPlayLength();

	//if we had a valid sequence length before and our new end time is shorter
	//then we need to clamp.
	return (SequenceLength > 0.f && NewEndTime < SequenceLength);
}

void FMotionModel_AnimSequenceBase::RefreshSnapTimes()
{
	SnapTimes.Reset();
	for (const FAnimNotifyEvent& Notify : AnimSequenceBase->Notifies)
	{
		SnapTimes.Emplace(FSnapType::Notifies.Type, (double)Notify.GetTime());
		if (Notify.NotifyStateClass != nullptr)
		{
			SnapTimes.Emplace(FSnapType::Notifies.Type, (double)(Notify.GetTime() + Notify.GetDuration()));
		}
	}
}

void FMotionModel_AnimSequenceBase::RefreshNotifyTracks()
{
	AnimSequenceBase->InitializeNotifyTrack();

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

//void FMotionModel_AnimSequenceBase::RefreshCurveTracks()
//{
//	if (!CurveRoot.IsValid())
//	{
//		// Add a root track for curves
//		CurveRoot = MakeShared<FMotionTimelineTrack_Curves>(SharedThis(this));
//	}
//
//	CurveRoot->ClearChildren();
//	RootTracks.Add(CurveRoot.ToSharedRef());
//
//	// Next add a track for each float curve
//	for (FFloatCurve& FloatCurve : AnimSequenceBase->RawCurveData.FloatCurves)
//	{
//		CurveRoot->AddChild(MakeShared<FMotionTimelineTrack_FloatCurve>(FloatCurve, SharedThis(this)));
//	}
//
//	UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimSequenceBase);
//	if (AnimSequence)
//	{
//		if (!AdditiveRoot.IsValid())
//		{
//			// Add a root track for additive layers
//			AdditiveRoot = MakeShared<FMotionTimelineTrack>(LOCTEXT("AdditiveLayerTrackList_Title", "Additive Layer Tracks"), LOCTEXT("AdditiveLayerTrackList_Tooltip", "Additive modifications to bone transforms"), SharedThis(this), true);
//		}
//
//		AdditiveRoot->ClearChildren();
//		RootTracks.Add(AdditiveRoot.ToSharedRef());
//
//		// Next add a track for each transform curve
//		for (FTransformCurve& TransformCurve : AnimSequence->RawCurveData.TransformCurves)
//		{
//			TSharedRef<FMotionTimelineTrack_TransformCurve> TransformCurveTrack = MakeShared<FMotionTimelineTrack_TransformCurve>(TransformCurve, SharedThis(this));
//			TransformCurveTrack->SetExpanded(false);
//			AdditiveRoot->AddChild(TransformCurveTrack);
//
//			FText TransformName = FMotionTimelineTrack_TransformCurve::GetTransformCurveName(AsShared(), TransformCurve.Name);
//			FLinearColor TransformColor = TransformCurve.GetColor();
//			FLinearColor XColor = FLinearColor::Red;
//			FLinearColor YColor = FLinearColor::Green;
//			FLinearColor ZColor = FLinearColor::Blue;
//			FText XName = LOCTEXT("VectorXTrackName", "X");
//			FText YName = LOCTEXT("VectorYTrackName", "Y");
//			FText ZName = LOCTEXT("VectorZTrackName", "Z");
//
//			FText VectorFormat = LOCTEXT("TransformVectorFormat", "{0}.{1}");
//			FText TranslationName = LOCTEXT("TransformTranslationTrackName", "Translation");
//			TSharedRef<FMotionTimelineTrack_VectorCurve> TranslationCurveTrack = MakeShared<FMotionTimelineTrack_VectorCurve>(TransformCurve.TranslationCurve, TransformCurve.Name, 0, ERawCurveTrackTypes::RCT_Transform, TranslationName, FText::Format(VectorFormat, TransformName, TranslationName), TransformColor, SharedThis(this));
//			TranslationCurveTrack->SetExpanded(false);
//			TransformCurveTrack->AddChild(TranslationCurveTrack);
//
//			FText ComponentFormat = LOCTEXT("TransformComponentFormat", "{0}.{1}.{2}");
//			TranslationCurveTrack->AddChild(MakeShared<FMotionTimelineTrack_Curve>(TransformCurve.TranslationCurve.FloatCurves[0], TransformCurve.Name, 0, ERawCurveTrackTypes::RCT_Transform, XName, FText::Format(ComponentFormat, TransformName, TranslationName, XName), XColor, XColor, SharedThis(this)));
//			TranslationCurveTrack->AddChild(MakeShared<FMotionTimelineTrack_Curve>(TransformCurve.TranslationCurve.FloatCurves[1], TransformCurve.Name, 1, ERawCurveTrackTypes::RCT_Transform, YName, FText::Format(ComponentFormat, TransformName, TranslationName, YName), YColor, YColor, SharedThis(this)));
//			TranslationCurveTrack->AddChild(MakeShared<FMotionTimelineTrack_Curve>(TransformCurve.TranslationCurve.FloatCurves[2], TransformCurve.Name, 2, ERawCurveTrackTypes::RCT_Transform, ZName, FText::Format(ComponentFormat, TransformName, TranslationName, ZName), ZColor, ZColor, SharedThis(this)));
//
//			FText RollName = LOCTEXT("RotationRollTrackName", "Roll");
//			FText PitchName = LOCTEXT("RotationPitchTrackName", "Pitch");
//			FText YawName = LOCTEXT("RotationYawTrackName", "Yaw");
//			FText RotationName = LOCTEXT("TransformRotationTrackName", "Rotation");
//			TSharedRef<FMotionTimelineTrack_VectorCurve> RotationCurveTrack = MakeShared<FMotionTimelineTrack_VectorCurve>(TransformCurve.RotationCurve, TransformCurve.Name, 3, ERawCurveTrackTypes::RCT_Transform, RotationName, FText::Format(VectorFormat, TransformName, RotationName), TransformColor, SharedThis(this));
//			RotationCurveTrack->SetExpanded(false);
//			TransformCurveTrack->AddChild(RotationCurveTrack);
//			RotationCurveTrack->AddChild(MakeShared<FMotionTimelineTrack_Curve>(TransformCurve.RotationCurve.FloatCurves[0], TransformCurve.Name, 3, ERawCurveTrackTypes::RCT_Transform, RollName, FText::Format(ComponentFormat, TransformName, RotationName, RollName), XColor, XColor, SharedThis(this)));
//			RotationCurveTrack->AddChild(MakeShared<FMotionTimelineTrack_Curve>(TransformCurve.RotationCurve.FloatCurves[1], TransformCurve.Name, 4, ERawCurveTrackTypes::RCT_Transform, PitchName, FText::Format(ComponentFormat, TransformName, RotationName, PitchName), YColor, YColor, SharedThis(this)));
//			RotationCurveTrack->AddChild(MakeShared<FMotionTimelineTrack_Curve>(TransformCurve.RotationCurve.FloatCurves[2], TransformCurve.Name, 5, ERawCurveTrackTypes::RCT_Transform, YawName, FText::Format(ComponentFormat, TransformName, RotationName, YawName), ZColor, ZColor, SharedThis(this)));
//
//			FText ScaleName = LOCTEXT("TransformScaleTrackName", "Scale");
//			TSharedRef<FMotionTimelineTrack_VectorCurve> ScaleCurveTrack = MakeShared<FMotionTimelineTrack_VectorCurve>(TransformCurve.ScaleCurve, TransformCurve.Name, 6, ERawCurveTrackTypes::RCT_Transform, ScaleName, FText::Format(VectorFormat, TransformName, ScaleName), TransformColor, SharedThis(this));
//			ScaleCurveTrack->SetExpanded(false);
//			TransformCurveTrack->AddChild(ScaleCurveTrack);
//			ScaleCurveTrack->AddChild(MakeShared<FMotionTimelineTrack_Curve>(TransformCurve.ScaleCurve.FloatCurves[0], TransformCurve.Name, 6, ERawCurveTrackTypes::RCT_Transform, XName, FText::Format(ComponentFormat, TransformName, ScaleName, XName), XColor, XColor, SharedThis(this)));
//			ScaleCurveTrack->AddChild(MakeShared<FMotionTimelineTrack_Curve>(TransformCurve.ScaleCurve.FloatCurves[1], TransformCurve.Name, 7, ERawCurveTrackTypes::RCT_Transform, YName, FText::Format(ComponentFormat, TransformName, ScaleName, YName), YColor, YColor, SharedThis(this)));
//			ScaleCurveTrack->AddChild(MakeShared<FMotionTimelineTrack_Curve>(TransformCurve.ScaleCurve.FloatCurves[2], TransformCurve.Name, 8, ERawCurveTrackTypes::RCT_Transform, ZName, FText::Format(ComponentFormat, TransformName, ScaleName, ZName), ZColor, ZColor, SharedThis(this)));
//		}
//	}
//}

//void FMotionModel_AnimSequenceBase::EditSelectedCurves()
//{
//	TArray<IAnimationEditor::FCurveEditInfo> EditCurveInfo;
//	for (TSharedRef<FMotionTimelineTrack>& SelectedTrack : SelectedTracks)
//	{
//		if (SelectedTrack->IsA<FMotionTimelineTrack_Curve>())
//		{
//			TSharedRef<FMotionTimelineTrack_Curve> CurveTrack = StaticCastSharedRef<FMotionTimelineTrack_Curve>(SelectedTrack);
//			const TArray<FRichCurve*> Curves = CurveTrack->GetCurves();
//			int32 NumCurves = Curves.Num();
//			for (int32 CurveIndex = 0; CurveIndex < NumCurves; ++CurveIndex)
//			{
//				if (CurveTrack->CanEditCurve(CurveIndex))
//				{
//					FText FullName = CurveTrack->GetFullCurveName(CurveIndex);
//					FLinearColor Color = CurveTrack->GetCurveColor(CurveIndex);
//					FSmartName Name;
//					ERawCurveTrackTypes Type;
//					int32 EditCurveIndex;
//					CurveTrack->GetCurveEditInfo(CurveIndex, Name, Type, EditCurveIndex);
//					FSimpleDelegate OnCurveChanged = FSimpleDelegate::CreateSP(&CurveTrack.Get(), &FMotionTimelineTrack_Curve::HandleCurveChanged);
//					EditCurveInfo.AddUnique(IAnimationEditor::FCurveEditInfo(FullName, Color, Name, Type, EditCurveIndex, OnCurveChanged));
//				}
//			}
//		}
//	}
//
//	if (EditCurveInfo.Num() > 0)
//	{
//		OnEditCurves.ExecuteIfBound(AnimSequenceBase, EditCurveInfo, nullptr);
//	}
//}
//
//bool FMotionModel_AnimSequenceBase::CanEditSelectedCurves() const
//{
//	for (const TSharedRef<FMotionTimelineTrack>& SelectedTrack : SelectedTracks)
//	{
//		if (SelectedTrack->IsA<FMotionTimelineTrack_Curve>())
//		{
//			TSharedRef<FMotionTimelineTrack_Curve> CurveTrack = StaticCastSharedRef<FMotionTimelineTrack_Curve>(SelectedTrack);
//			const TArray<FRichCurve*>& Curves = CurveTrack->GetCurves();
//			for (int32 CurveIndex = 0; CurveIndex < Curves.Num(); ++CurveIndex)
//			{
//				if (CurveTrack->CanEditCurve(CurveIndex))
//				{
//					return true;
//				}
//			}
//		}
//	}
//
//	return false;
//}
//
//
//
//void FMotionModel_AnimSequenceBase::RemoveSelectedCurves()
//{
//	FScopedTransaction Transaction(LOCTEXT("CurvePanel_RemoveCurves", "Remove Curves"));
//
//	AnimSequenceBase->Modify(true);
//
//	bool bDeletedCurve = false;
//
//	for (TSharedRef<FMotionTimelineTrack>& SelectedTrack : SelectedTracks)
//	{
//		if (SelectedTrack->IsA<FMotionTimelineTrack_FloatCurve>())
//		{
//			TSharedRef<FMotionTimelineTrack_FloatCurve> FloatCurveTrack = StaticCastSharedRef<FMotionTimelineTrack_FloatCurve>(SelectedTrack);
//
//			FFloatCurve& FloatCurve = FloatCurveTrack->GetFloatCurve();
//			FSmartName CurveName = FloatCurveTrack->GetName();
//
//			if (AnimSequenceBase->RawCurveData.GetCurveData(CurveName.UID))
//			{
//				FSmartName TrackName;
//				if (AnimSequenceBase->GetSkeleton()->GetSmartNameByUID(USkeleton::AnimCurveMappingName, CurveName.UID, TrackName))
//				{
//					// Stop editing this curve in the external editor window
//					FSmartName Name;
//					ERawCurveTrackTypes Type;
//					int32 CurveEditIndex;
//					FloatCurveTrack->GetCurveEditInfo(0, Name, Type, CurveEditIndex);
//					IAnimationEditor::FCurveEditInfo EditInfo(Name, Type, CurveEditIndex);
//					OnStopEditingCurves.ExecuteIfBound(TArray<IAnimationEditor::FCurveEditInfo>({ EditInfo }));
//
//					AnimSequenceBase->RawCurveData.DeleteCurveData(TrackName);
//					bDeletedCurve = true;
//				}
//			}
//		}
//		else if (SelectedTrack->IsA<FMotionTimelineTrack_TransformCurve>())
//		{
//			TSharedRef<FMotionTimelineTrack_TransformCurve> TransformCurveTrack = StaticCastSharedRef<FMotionTimelineTrack_TransformCurve>(SelectedTrack);
//
//			FTransformCurve& TransformCurve = TransformCurveTrack->GetTransformCurve();
//			FSmartName CurveName = TransformCurveTrack->GetName();
//
//			if (AnimSequenceBase->RawCurveData.GetCurveData(CurveName.UID, ERawCurveTrackTypes::RCT_Transform))
//			{
//				FSmartName CurveToDelete;
//				if (AnimSequenceBase->GetSkeleton()->GetSmartNameByUID(USkeleton::AnimTrackCurveMappingName, CurveName.UID, CurveToDelete))
//				{
//					// Stop editing these curves in the external editor window
//					TArray<IAnimationEditor::FCurveEditInfo> CurveEditInfo;
//					for (int32 CurveIndex = 0; CurveIndex < TransformCurveTrack->GetCurves().Num(); ++CurveIndex)
//					{
//						FSmartName Name;
//						ERawCurveTrackTypes Type;
//						int32 CurveEditIndex;
//						TransformCurveTrack->GetCurveEditInfo(CurveIndex, Name, Type, CurveEditIndex);
//						IAnimationEditor::FCurveEditInfo EditInfo(Name, Type, CurveEditIndex);
//						CurveEditInfo.Add(EditInfo);
//					}
//
//					OnStopEditingCurves.ExecuteIfBound(CurveEditInfo);
//
//					AnimSequenceBase->RawCurveData.DeleteCurveData(CurveToDelete, ERawCurveTrackTypes::RCT_Transform);
//
//					if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimSequenceBase))
//					{
//						AnimSequence->bNeedsRebake = true;
//					}
//
//					bDeletedCurve = true;
//				}
//			}
//		}
//	}
//
//	if (bDeletedCurve)
//	{
//		AnimSequenceBase->MarkRawDataAsModified();
//		AnimSequenceBase->PostEditChange();
//
//		if (GetPreviewScene()->GetPreviewMeshComponent()->PreviewInstance != nullptr)
//		{
//			GetPreviewScene()->GetPreviewMeshComponent()->PreviewInstance->RefreshCurveBoneControllers();
//		}
//	}
//
//	RefreshTracks();
//}

void FMotionModel_AnimSequenceBase::SetDisplayFormat(EFrameNumberDisplayFormats InFormat)
{
	GetMutableDefault<UPersonaOptions>()->TimelineDisplayFormat = InFormat;
}



bool FMotionModel_AnimSequenceBase::IsDisplayFormatChecked(EFrameNumberDisplayFormats InFormat) const
{
	return GetDefault<UPersonaOptions>()->TimelineDisplayFormat == InFormat;
}

void FMotionModel_AnimSequenceBase::ToggleDisplayPercentage()
{
	GetMutableDefault<UPersonaOptions>()->bTimelineDisplayPercentage = !GetDefault<UPersonaOptions>()->bTimelineDisplayPercentage;
}



bool FMotionModel_AnimSequenceBase::IsDisplayPercentageChecked() const
{
	return GetDefault<UPersonaOptions>()->bTimelineDisplayPercentage;
}

void FMotionModel_AnimSequenceBase::ToggleDisplaySecondary()
{
	GetMutableDefault<UPersonaOptions>()->bTimelineDisplayFormatSecondary = !GetDefault<UPersonaOptions>()->bTimelineDisplayFormatSecondary;
}

bool FMotionModel_AnimSequenceBase::IsDisplaySecondaryChecked() const
{
	return GetDefault<UPersonaOptions>()->bTimelineDisplayFormatSecondary;
}

void FMotionModel_AnimSequenceBase::HandleUndoRedo()
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