//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "MotionModel.h"

#include "PreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Controls/MotionTimelineTrack.h"
#include "Data/MotionAnimAsset.h"
#include "AnimPreviewInstance.h"
#include "Objects/MotionAnimObject.h"

#define LOCTEXT_NAMESPACE "FMotionModel"

const FMotionModel::FSnapType FMotionModel::FSnapType::Frames("Frames", LOCTEXT("FramesSnapName", "Frames"), [](const FMotionModel& InModel, double InTime)
	{
		// Round to nearest frame
		const double FrameRate = InModel.GetFrameRate();
		if (FrameRate > 0)
		{
			return FMath::RoundToDouble(InTime * FrameRate) / FrameRate;
		}

		return InTime;
	});

const FMotionModel::FSnapType FMotionModel::FSnapType::Notifies("Notifies", LOCTEXT("NotifiesSnapName", "Notifies"));

const FMotionModel::FSnapType FMotionModel::FSnapType::CompositeSegment("CompositeSegment", LOCTEXT("CompositeSegmentSnapName", "Composite Segments"));

const FMotionModel::FSnapType FMotionModel::FSnapType::MontageSection("MontageSection", LOCTEXT("MontageSectionSnapName", "Montage Sections"));

FMotionModel::FMotionModel(TObjectPtr<UMotionAnimObject> InMotionAnim, UDebugSkelMeshComponent* InDebugSkelMesh)
	: MotionAnim(InMotionAnim),
	DebugMesh(InDebugSkelMesh)
{
	SetViewRange(TRange<double>(0.0, MotionAnim->GetPlayLength()));
	SetPlaybackRange(TRange<double>(0.0f, MotionAnim->GetPlayLength()));
}

#if ENGINE_MINOR_VERSION > 2
FString FMotionModel::GetReferencerName() const
{
	return TEXT("FMotionModel");
}
#endif

void FMotionModel::Initialize()
{

}

FAnimatedRange FMotionModel::GetViewRange() const
{
	return ViewRange;
}

void FMotionModel::SetViewRange(TRange<double> InRange)
{
	ViewRange = InRange;

	if (WorkingRange.HasLowerBound() && WorkingRange.HasUpperBound())
	{
		WorkingRange = TRange<double>::Hull(WorkingRange, ViewRange);
	}
	else
	{
		WorkingRange = ViewRange;
	}
}

void FMotionModel::SetPlaybackRange(TRange<double> InRange)
{
	PlaybackRange = InRange;
}

FAnimatedRange FMotionModel::GetWorkingRange() const
{
	return WorkingRange;
}

TRange<FFrameNumber> FMotionModel::GetPlaybackRange() const
{
	const int32 Resolution = GetTickResolution();
	return TRange<FFrameNumber>(FFrameNumber(FMath::RoundToInt32(PlaybackRange.GetLowerBoundValue()
		* static_cast<double>(Resolution))), FFrameNumber(FMath::RoundToInt32(
			PlaybackRange.GetUpperBoundValue() * static_cast<double>(Resolution))));
}

void FMotionModel::HandleViewRangeChanged(TRange<double> InRange, EViewRangeInterpolation InInterpolation)
{
	SetViewRange(InRange);
}

void FMotionModel::HandleWorkingRangeChanged(TRange<double> InRange)
{
	WorkingRange = InRange;
}

FFrameNumber FMotionModel::GetScrubPosition() const
{
	if (DebugMesh && DebugMesh->IsPreviewOn())
	{
		return FFrameNumber(FMath::RoundToInt32(DebugMesh->PreviewInstance->GetCurrentTime() 
			* static_cast<double>(GetTickResolution())));
	}

	return FFrameNumber(0);
}

float FMotionModel::GetScrubTime() const
{
	if (DebugMesh && DebugMesh->IsPreviewOn())
	{
		return DebugMesh->PreviewInstance->GetCurrentTime();
	}

	return 0.0f;
}

void FMotionModel::SetScrubPosition(FFrameTime NewScrubPosition) const
{
	if (DebugMesh && DebugMesh->IsPreviewOn())
	{
		DebugMesh->PreviewInstance->SetPosition(NewScrubPosition.AsDecimal() / (double)GetTickResolution());
	}
}

double FMotionModel::GetFrameRate() const
{
	if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(GetAnimAsset()))
	{
		return (double)AnimSequence->GetSamplingFrameRate().AsDecimal();
	}
	else
	{
		return 30.0;
	}
}

int32 FMotionModel::GetTickResolution() const
{
	return FMath::RoundToInt((double)GetDefault<UPersonaOptions>()->TimelineScrubSnapValue * GetFrameRate());
}

UAnimationAsset* FMotionModel::GetAnimAsset() const
{
	if(!MotionAnim)
		return nullptr;

	return MotionAnim->AnimAsset;
}

void FMotionModel::RefreshTracks()
{
	//Override
}

void FMotionModel::UpdateRange()
{
	//Override
}

TSharedRef<FUICommandList> FMotionModel::GetCommandList() const
{
	return WeakCommandList.Pin().ToSharedRef();
}

bool FMotionModel::IsTrackSelected(const TSharedRef<FMotionTimelineTrack>& InTrack) const
{
	return SelectedTracks.Find(InTrack) != nullptr;
}

void FMotionModel::ClearTrackSelection()
{
	SelectedTracks.Empty();
}

void FMotionModel::SetTrackSelected(const TSharedRef<FMotionTimelineTrack>& InTrack, bool bIsSelected)
{
	if (bIsSelected)
	{
		SelectedTracks.Add(InTrack);
	}
	else
	{
		SelectedTracks.Remove(InTrack);
	}
}

UObject* FMotionModel::ShowInDetailsView(UClass* EdClass)
{
	/*UObject* Obj = EditorObjectTracker.GetEditorObjectForClass(EdClass);
	if (Obj != nullptr)
	{
		if (Obj->IsA(UEditorAnimBaseObj::StaticClass()))
		{
			if (!bIsSelecting)
			{
				TGuardValue<bool> GuardValue(bIsSelecting, true);

				ClearTrackSelection();

				UEditorAnimBaseObj* EdObj = Cast<UEditorAnimBaseObj>(Obj);
				InitDetailsViewEditorObject(EdObj);

				TArray<UObject*> Objects;
				Objects.Add(EdObj);
				OnSelectObjects.ExecuteIfBound(Objects);

				OnHandleObjectsSelectedDelegate.Broadcast(Objects);
			}
		}
	}
	return Obj;*/

	return nullptr;
}

void FMotionModel::SelectObjects(const TArray<UObject*>& Objects)
{
	TGuardValue<bool> GuardValue(bIsSelecting, true);
	OnSelectObjects.ExecuteIfBound(Objects);

	OnHandleObjectsSelectedDelegate.Broadcast(Objects);
}

void FMotionModel::ClearDetailsView()
{
	if (!bIsSelecting)
	{
		TGuardValue<bool> GuardValue(bIsSelecting, true);

		const TArray<UObject*> Objects;
		OnSelectObjects.ExecuteIfBound(Objects);
		OnHandleObjectsSelectedDelegate.Broadcast(Objects);
	}
}

void FMotionModel::AddReferencedObjects(FReferenceCollector& Collector)
{
	//EditorObjectTracker.AddReferencedObjects(Collector);
}

void FMotionModel::RecalculateSequenceLength()
{
	// if (UAnimSequenceBase* AnimSequenceBase = Cast<UAnimSequenceBase>(MotionAnim->AnimAsset))
	// {
		//AnimSequenceBase->ClampNotifiesAtEndOfSequence();
	//}
}

float FMotionModel::CalculateSequenceLengthOfEditorObjects() const
{
	return GetPlayLength();
}

float FMotionModel::GetPlayLength() const
{
	if (UAnimSequenceBase* AnimSequenceBase = Cast<UAnimSequenceBase>(MotionAnim->AnimAsset))
	{
		return AnimSequenceBase->GetPlayLength();
	}

	return 0.0f;
}

void FMotionModel::InitDetailsViewEditorObjects(UEditorAnimBaseObj* EdObj)
{
//Override
}

void FMotionModel::SetEditableTime(int32 TimeIndex, double Time, bool bIsDragging)
{
	EditableTimes[TimeIndex] = FMath::Clamp(Time, 0.0, (double)CalculateSequenceLengthOfEditorObjects());

	OnSetEditableTime(TimeIndex, EditableTimes[TimeIndex], bIsDragging);
}

void FMotionModel::OnSetEditableTime(int32 TimeIndex, double Time, bool bIsDragging)
{
//Override
}

bool FMotionModel::Snap(float& InOutTime, float InSnapMargin, TArrayView<const FName> InSkippedSnapTypes) const
{
	double DoubleTime = (double)InOutTime;
	bool bResult = Snap(DoubleTime, (double)InSnapMargin, InSkippedSnapTypes);
	InOutTime = DoubleTime;
	return bResult;
}

bool FMotionModel::Snap(double& InOutTime, double InSnapMargin, TArrayView<const FName> InSkippedSnapTypes) const
{
	InSnapMargin = FMath::Max(InSnapMargin, (double)KINDA_SMALL_NUMBER);

	double ClosestDelta = DBL_MAX;
	double ClosestSnapTime = DBL_MAX;

	// Check for enabled snap functions first
	for (const TPair<FName, FSnapType>& SnapTypePair : SnapTypes)
	{
		if (SnapTypePair.Value.SnapFunction != nullptr)
		{
			if (IsSnapChecked(SnapTypePair.Value.Type))
			{
				if (!InSkippedSnapTypes.Contains(SnapTypePair.Value.Type))
				{
					double SnappedTime = SnapTypePair.Value.SnapFunction(*this, InOutTime);
					if (SnappedTime != InOutTime)
					{
						double Delta = FMath::Abs(SnappedTime - InOutTime);
						if (Delta < InSnapMargin && Delta < ClosestDelta)
						{
							ClosestDelta = Delta;
							ClosestSnapTime = SnappedTime;
						}
					}
				}
			}
		}
	}

	// Find the closest in-range enabled snap time
	for (const FSnapTime& SnapTime : SnapTimes)
	{
		const double Delta = FMath::Abs(SnapTime.Time - InOutTime);
		if (Delta < InSnapMargin && Delta < ClosestDelta)
		{
			if (!InSkippedSnapTypes.Contains(SnapTime.Type))
			{
				if (const FSnapType* SnapType = SnapTypes.Find(SnapTime.Type))
				{
					if (IsSnapChecked(SnapTime.Type))
					{
						ClosestDelta = Delta;
						ClosestSnapTime = SnapTime.Time;
					}
				}
			}
		}
	}

	if (ClosestDelta != DBL_MAX)
	{
		InOutTime = ClosestSnapTime;
		return true;
	}

	return false;
}

void FMotionModel::ToggleSnap(FName InSnapName)
{
	if (IsSnapChecked(InSnapName))
	{
		GetMutableDefault<UPersonaOptions>()->TimelineEnabledSnaps.Remove(InSnapName);
	}
	else
	{
		GetMutableDefault<UPersonaOptions>()->TimelineEnabledSnaps.AddUnique(InSnapName);
	}
}

bool FMotionModel::IsSnapChecked(FName InSnapName) const
{
	return GetDefault<UPersonaOptions>()->TimelineEnabledSnaps.Contains(InSnapName);
}

bool FMotionModel::IsSnapAvailable(FName InSnapName) const
{
	return SnapTypes.Find(InSnapName) != nullptr;
}

void FMotionModel::BuildContextMenu(FMenuBuilder& InMenuBuilder)
{
	TSet<FName> ExistingMenuTypes;
	for (const TSharedRef<FMotionTimelineTrack>& SelectedItem : SelectedTracks)
	{
		SelectedItem->AddToContextMenu(InMenuBuilder, ExistingMenuTypes);
	}
}

#undef LOCTEXT_NAMESPACE