//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Controls/MotionTimeSliderController.h"


class FPaintArgs;
class FSlateWindowElementList;
class FMotionTimeSliderController;

/**
 * An overlay that displays global information in the track area
 */
class SMotionTimelineOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMotionTimelineOverlay)
		: _DisplayTickLines(true)
		, _DisplayScrubPosition(false)
		{}

	SLATE_ATTRIBUTE(bool, DisplayTickLines)
	SLATE_ATTRIBUTE(bool, DisplayScrubPosition)
	SLATE_ATTRIBUTE(FPaintPlaybackRangeArgs, PaintPlaybackRangeArgs)

	SLATE_END_ARGS()

private:
	/** Controller for manipulating time */
	TSharedPtr<FMotionTimeSliderController> TimeSliderController;
	/** Whether or not to display the scrub position */
	TAttribute<bool> bDisplayScrubPosition;
	/** Whether or not to display tick lines */
	TAttribute<bool> bDisplayTickLines;
	/** User-supplied options for drawing playback range */
	TAttribute<FPaintPlaybackRangeArgs> PaintPlaybackRangeArgs;

public:
	void Construct(const FArguments& InArgs, TSharedRef<FMotionTimeSliderController> InTimeSliderController);

private:
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, 
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, 
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
};