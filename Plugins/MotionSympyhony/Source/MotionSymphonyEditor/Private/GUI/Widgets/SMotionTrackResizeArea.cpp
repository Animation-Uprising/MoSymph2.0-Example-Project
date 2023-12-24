//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "SMotionTrackResizeArea.h"
#include "Controls/MotionTimelineTrack.h"
#include "Widgets/Layout/SBox.h"

void SMotionTrackResizeArea::Construct(const FArguments& InArgs, TWeakPtr<FMotionTimelineTrack> InTrack)
{
	Track = InTrack;

	ChildSlot
		[
			SNew(SBox)
			.HeightOverride(5.f)
		];
}

FReply SMotionTrackResizeArea::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<FMotionTimelineTrack> ResizeTarget = Track.Pin();
	if (ResizeTarget.IsValid() && MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		DragParameters = FDragParameters(ResizeTarget->GetHeight(), MouseEvent.GetScreenSpacePosition().Y);
		return FReply::Handled().CaptureMouse(AsShared());
	}
	return FReply::Handled();
}

FReply SMotionTrackResizeArea::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	DragParameters.Reset();
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SMotionTrackResizeArea::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (DragParameters.IsSet() && HasMouseCapture())
	{
		float NewHeight = DragParameters->OriginalHeight + (MouseEvent.GetScreenSpacePosition().Y - DragParameters->DragStartY);

		TSharedPtr<FMotionTimelineTrack> ResizeTarget = Track.Pin();
		if (ResizeTarget.IsValid() && FMath::RoundToInt(NewHeight) != FMath::RoundToInt(DragParameters->OriginalHeight))
		{
			ResizeTarget->SetHeight(NewHeight);
		}
	}

	return FReply::Handled();
}

FCursorReply SMotionTrackResizeArea::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
}
