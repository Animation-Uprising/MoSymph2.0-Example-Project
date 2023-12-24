//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "SMotionTimelineOverlay.h"

void SMotionTimelineOverlay::Construct(const FArguments& InArgs, TSharedRef<FMotionTimeSliderController> InTimeSliderController)
{
	bDisplayScrubPosition = InArgs._DisplayScrubPosition;
	bDisplayTickLines = InArgs._DisplayTickLines;
	PaintPlaybackRangeArgs = InArgs._PaintPlaybackRangeArgs;
	TimeSliderController = InTimeSliderController;
}

int32 SMotionTimelineOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, 
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, 
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FPaintViewAreaArgs PaintArgs;
	PaintArgs.bDisplayTickLines = bDisplayTickLines.Get();
	PaintArgs.bDisplayScrubPosition = bDisplayScrubPosition.Get();

	if (PaintPlaybackRangeArgs.IsSet())
	{
		PaintArgs.PlaybackRangeArgs = PaintPlaybackRangeArgs.Get();
	}

	TimeSliderController->OnPaintViewArea(AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, ShouldBeEnabled(bParentEnabled), PaintArgs);

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, 
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}
