//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "SMotionTrackArea.h"
#include "Types/PaintArgs.h"
#include "Layout/ArrangedChildren.h"
#include "Rendering/DrawElements.h"
#include "Layout/LayoutUtils.h"
#include "Widgets/SWeakWidget.h"
#include "EditorStyleSet.h"
#include "SMotionTrack.h"
#include "SMotionOutliner.h"
#include "Controls/MotionTimelineTrack.h"
#include "Controls/MotionModel.h"
#include "Controls/MotionTimeSliderController.h"

FMotionTrackAreaSlot::FMotionTrackAreaSlot(const TSharedPtr<SMotionTrack>& InSlotContent)
{
	TrackWidget = InSlotContent;

	HAlignment = HAlign_Fill;
	VAlignment = VAlign_Top;

	AttachWidget(
		SNew(SWeakWidget)
		.Clipping(EWidgetClipping::ClipToBounds)
		.PossiblyNullContent(InSlotContent)
	);
}


float FMotionTrackAreaSlot::GetVerticalOffset() const
{
	TSharedPtr<SMotionTrack> PinnedTrackWidget = TrackWidget.Pin();
	return PinnedTrackWidget.IsValid() ? PinnedTrackWidget->GetPhysicalPosition() : 0.f;
}

EHorizontalAlignment FMotionTrackAreaSlot::GetHorizontalAlignment() const
{
	return HAlignment;
}

EVerticalAlignment FMotionTrackAreaSlot::GetVerticalAlignment() const
{
	return VAlignment;
}

void SMotionTrackArea::Construct(const FArguments& InArgs, const TSharedRef<FMotionModel>& InMotionModel, 
                                 const TSharedRef<FMotionTimeSliderController>& InTimeSliderController)
{
	WeakModel = InMotionModel;
	WeakTimeSliderController = InTimeSliderController;
}

void SMotionTrackArea::Empty()
{
	TrackSlots.Empty();
	Children.Empty();
}

void SMotionTrackArea::AddTrackSlot(const TSharedRef<FMotionTimelineTrack>& InTrack, const TSharedPtr<SMotionTrack>& InSlot)
{
	TrackSlots.FindOrAdd(InTrack, InSlot);
	Children.AddSlot(FMotionTrackAreaSlot::FSlotArguments(MakeUnique<FMotionTrackAreaSlot>(InSlot)));
}

TSharedPtr<SMotionTrack> SMotionTrackArea::FindTrackSlot(const TSharedRef<FMotionTimelineTrack>& InTrack)
{
	TrackSlots.Remove(TWeakPtr<FMotionTimelineTrack>());

	return TrackSlots.FindRef(InTrack).Pin();
}	

void SMotionTrackArea::SetOutliner(const TSharedPtr<SMotionOutliner> InOutliner)
{
	WeakOutliner = InOutliner;
}

FReply SMotionTrackArea::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<FMotionTimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (TimeSliderController.IsValid())
	{
		WeakOutliner.Pin()->ClearSelection();
		WeakModel.Pin()->ClearDetailsView();

		TimeSliderController->OnMouseButtonDown(*this, MyGeometry, MouseEvent);
	}

	return FReply::Unhandled();
}

FReply SMotionTrackArea::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<FMotionTimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (TimeSliderController.IsValid())
	{
		return WeakTimeSliderController.Pin()->OnMouseButtonUp(*this, MyGeometry, MouseEvent);
	}

	return FReply::Unhandled();
}

FReply SMotionTrackArea::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	UpdateHoverStates(MyGeometry, MouseEvent);

	TSharedPtr<FMotionTimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (TimeSliderController.IsValid())
	{
		FReply Reply = WeakTimeSliderController.Pin()->OnMouseMove(*this, MyGeometry, MouseEvent);

		// Handle right click scrolling on the track area
		if (Reply.IsEventHandled())
		{
			if (MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton) && HasMouseCapture())
			{
				WeakOutliner.Pin()->ScrollByDelta(-MouseEvent.GetCursorDelta().Y);
			}
		}

		return Reply;
	}

	return FReply::Unhandled();
}

void SMotionTrackArea::OnMouseLeave(const FPointerEvent& MouseEvent)
{

}

FReply SMotionTrackArea::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<FMotionTimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
	if (TimeSliderController.IsValid())
	{
		FReply Reply = WeakTimeSliderController.Pin()->OnMouseWheel(*this, MyGeometry, MouseEvent);
		if (Reply.IsEventHandled())
		{
			return Reply;
		}

		const float ScrollAmount = -MouseEvent.GetWheelDelta() * GetGlobalScrollAmount();
		WeakOutliner.Pin()->ScrollByDelta(ScrollAmount);

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

int32 SMotionTrackArea::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// paint the child widgets
	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildren(AllottedGeometry, ArrangedChildren);

	const FPaintArgs NewArgs = Args.WithNewParent(this);

	for (int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex)
	{
		FArrangedWidget& CurWidget = ArrangedChildren[ChildIndex];
		FSlateRect ChildClipRect = MyCullingRect.IntersectionWith(CurWidget.Geometry.GetLayoutBoundingRect());
		const int32 ThisWidgetLayerId = CurWidget.Widget->Paint(NewArgs, CurWidget.Geometry, ChildClipRect, OutDrawElements, LayerId + 2, InWidgetStyle, bParentEnabled);

		LayerId = FMath::Max(LayerId, ThisWidgetLayerId);
	}

	return LayerId;
}

FCursorReply SMotionTrackArea::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (CursorEvent.IsMouseButtonDown(EKeys::RightMouseButton) && HasMouseCapture())
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
	}
	else
	{
		TSharedPtr<FMotionTimeSliderController> TimeSliderController = WeakTimeSliderController.Pin();
		if (TimeSliderController.IsValid())
		{
			return TimeSliderController->OnCursorQuery(SharedThis(this), MyGeometry, CursorEvent);
		}
	}

	return FCursorReply::Unhandled();
}

void SMotionTrackArea::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	CachedGeometry = AllottedGeometry;

	for (int32 Index = 0; Index < Children.Num();)
	{
		if (!StaticCastSharedRef<SWeakWidget>(Children[Index].GetWidget())->ChildWidgetIsValid())
		{
			Children.RemoveAt(Index);
		}
		else
		{
			++Index;
		}
	}
}

void SMotionTrackArea::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		const FMotionTrackAreaSlot& CurChild = Children[ChildIndex];

		const EVisibility ChildVisibility = CurChild.GetWidget()->GetVisibility();
		if (!ArrangedChildren.Accepts(ChildVisibility))
		{
			continue;
		}

		const FMargin Padding(0, CurChild.GetVerticalOffset(), 0, 0);

		AlignmentArrangeResult XResult = AlignChild<Orient_Horizontal>(AllottedGeometry.GetLocalSize().X, CurChild, Padding, 1.0f, false);
		AlignmentArrangeResult YResult = AlignChild<Orient_Vertical>(AllottedGeometry.GetLocalSize().Y, CurChild, Padding, 1.0f, false);

		ArrangedChildren.AddWidget(ChildVisibility,
			AllottedGeometry.MakeChild(
				CurChild.GetWidget(),
				FVector2D(XResult.Offset, YResult.Offset),
				FVector2D(XResult.Size, YResult.Size)
			)
		);
	}
}

FVector2D SMotionTrackArea::ComputeDesiredSize(float) const
{
	FVector2D MaxSize(0.0f, 0.0f);
	for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		const FMotionTrackAreaSlot& CurChild = Children[ChildIndex];

		const EVisibility ChildVisibilty = CurChild.GetWidget()->GetVisibility();
		if (ChildVisibilty != EVisibility::Collapsed)
		{
			FVector2D ChildDesiredSize = CurChild.GetWidget()->GetDesiredSize();
			MaxSize.X = FMath::Max(MaxSize.X, ChildDesiredSize.X);
			MaxSize.Y = FMath::Max(MaxSize.Y, ChildDesiredSize.Y);
		}
	}

	return MaxSize;
}

FChildren* SMotionTrackArea::GetChildren()
{
	return &Children;
}

void SMotionTrackArea::UpdateHoverStates(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{

}