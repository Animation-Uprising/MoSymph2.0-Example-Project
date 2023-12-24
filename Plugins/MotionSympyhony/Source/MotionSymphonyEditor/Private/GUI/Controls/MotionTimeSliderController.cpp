//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Controls/MotionTimeSliderController.h"
#include "Fonts/SlateFontInfo.h"
#include "Rendering/DrawElements.h"
#include "Misc/Paths.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "MovieSceneTimeHelpers.h"
#include "CommonFrameRates.h"
#if ENGINE_MINOR_VERSION > 2
#include "TimeSliderArgs.h"
#endif
#include "Controls/MotionModel.h"
#include "Preferences/PersonaOptions.h"
#include "Animation/AnimMontage.h"

#define LOCTEXT_NAMESPACE "AnimTimeSliderController"

namespace MotionScrubConstants
{
	/** The minimum amount of pixels between each major ticks on the widget */
	const int32 MinPixelsPerDisplayTick = 12;

	/**The smallest number of units between between major tick marks */
	const float MinDisplayTickSpacing = 0.001f;

	/**The fraction of the current view range to scroll per unit delta  */
	const float ScrollPanFraction = 0.1f;

	/** The margin we use for snapping editable times */
	const double SnapMarginInPixels = 5.0;
}

/** Utility struct for converting between scrub range space and local/absolute screen space */
struct FMotionTimeSliderController::FScrubRangeToScreen
{
	double ViewStart;
	float  PixelsPerInput;

	FScrubRangeToScreen(const TRange<double>& InViewInput, const FVector2D& InWidgetSize)
	{
		const float ViewInputRange = InViewInput.Size<double>();

		ViewStart = InViewInput.GetLowerBoundValue();
		PixelsPerInput = ViewInputRange > 0 ? (InWidgetSize.X / ViewInputRange) : 0;
	}

	/** Local Widget Space -> Curve Input domain. */
	double LocalXToInput(float ScreenX) const
	{
		return PixelsPerInput > 0 ? (ScreenX / PixelsPerInput) + ViewStart : ViewStart;
	}

	/** Curve Input domain -> local Widget Space */
	float InputToLocalX(double Input) const
	{
		return (Input - ViewStart) * PixelsPerInput;
	}
};

struct FMotionTimeSliderController::FDrawTickArgs
{
	/** Geometry of the area */
	FGeometry AllottedGeometry;
	/** Culling rect of the area */
	FSlateRect CullingRect;
	/** Color of each tick */
	FLinearColor TickColor;
	/** Offset in Y where to start the tick */
	float TickOffset;
	/** Height in of major ticks */
	float MajorTickHeight;
	/** Start layer for elements */
	int32 StartLayer;
	/** Draw effects to apply */
	ESlateDrawEffect DrawEffects;
	/** Whether or not to only draw major ticks */
	bool bOnlyDrawMajorTicks;
	/** Whether or not to mirror labels */
	bool bMirrorLabels;

};

/**
 * Gets the the next spacing value in the series
 * to determine a good spacing value
 * E.g, .001,.005,.010,.050,.100,.500,1.000,etc
 */
static float GetNextSpacing(uint32 CurrentStep)
{
	if (CurrentStep & 0x01)
	{
		// Odd numbers
		return FMath::Pow(10.f, 0.5f * ((float)(CurrentStep - 1)) + 1.f);
	}
	else
	{
		// Even numbers
		return 0.5f * FMath::Pow(10.f, 0.5f * ((float)(CurrentStep)) + 1.f);
	}
}

FMotionTimeSliderController::FMotionTimeSliderController(const FTimeSliderArgs& InArgs, TWeakPtr<FMotionModel> InWeakModel,
	TWeakPtr<SMotionTimeline> InWeakTimeline, TSharedPtr<INumericTypeInterface<double>> InSecondaryNumericTypeInterface)
	: WeakModel(InWeakModel),
	WeakTimeline(InWeakTimeline),
	TimeSliderArgs(InArgs),
	DistanceDragged(0.0f),
	MouseDragType(DRAG_NONE),
	bPanning(false),
	//DraggingTimeIndex(INDEX_NONE),
	SecondaryNumericTypeInterface(InSecondaryNumericTypeInterface)

{
	ScrubFillBrush = FAppStyle::GetBrush(TEXT("Sequencer.Timeline.ScrubFill"));
	ScrubHandleUpBrush = FAppStyle::GetBrush(TEXT("Sequencer.Timeline.VanillaScrubHandleUp"));
	ScrubHandleDownBrush = FAppStyle::GetBrush(TEXT("Sequencer.Timeline.VanillaScrubHandleDown"));
	EditableTimeBrush = FAppStyle::GetBrush(TEXT("AnimTimeline.SectionMarker"));
}



void FMotionTimeSliderController::ClampViewRange(double& NewRangeMin, double& NewRangeMax)
{
	bool bNeedsClampSet = false;
	double NewClampRangeMin = TimeSliderArgs.ClampRange.Get().GetLowerBoundValue();
	
	if(NewRangeMin < TimeSliderArgs.ClampRange.Get().GetLowerBoundValue())
	{
		NewClampRangeMin = NewRangeMin;
		bNeedsClampSet = true;
	}

	double NewClampRangeMax = TimeSliderArgs.ClampRange.Get().GetUpperBoundValue();
	if (NewRangeMax > TimeSliderArgs.ClampRange.Get().GetUpperBoundValue())
	{
		NewClampRangeMax = NewRangeMax;
		bNeedsClampSet = true;
	}

	if (bNeedsClampSet)
	{
		SetClampRange(NewClampRangeMin, NewClampRangeMax);
	}
}

bool FMotionTimeSliderController::ZoomByDelta(float InDelta, float ZoomBias /*= 0.5f*/)
{
	const TRange<double> LocalViewRange = TimeSliderArgs.ViewRange.Get().GetAnimationTarget();
	const double LocalViewRangeMax = LocalViewRange.GetUpperBoundValue();
	const double LocalViewRangeMin = LocalViewRange.GetLowerBoundValue();
	const double OutputViewSize = LocalViewRangeMax - LocalViewRangeMin;
	const double OutputChange = OutputViewSize * InDelta;

	double NewViewOutputMin = LocalViewRangeMin - (OutputChange * ZoomBias);
	double NewViewOutputMax = LocalViewRangeMax + (OutputChange * (1.f - ZoomBias));

	if (NewViewOutputMin < NewViewOutputMax)
	{
		ClampViewRange(NewViewOutputMin, NewViewOutputMax);
		SetViewRange(NewViewOutputMin, NewViewOutputMax, EViewRangeInterpolation::Animated);
		return true;
	}

	return false;
}

void FMotionTimeSliderController::PanByDelta(float InDelta)
{
	const TRange<double> LocalViewRange = TimeSliderArgs.ViewRange.Get().GetAnimationTarget();

	const double CurrentMin = LocalViewRange.GetLowerBoundValue();
	const double CurrentMax = LocalViewRange.GetUpperBoundValue();

	// Adjust the delta to be a percentage of the current range
	InDelta *= MotionScrubConstants::ScrollPanFraction * (CurrentMax - CurrentMin);

	double NewViewOutputMin = CurrentMin + InDelta;
	double NewViewOutputMax = CurrentMax + InDelta;

	ClampViewRange(NewViewOutputMin, NewViewOutputMax);
	SetViewRange(NewViewOutputMin, NewViewOutputMax, EViewRangeInterpolation::Animated);
}


FFrameTime FMotionTimeSliderController::GetFrameTimeFromMouse(const FGeometry& Geometry, FVector2D ScreenSpacePosition) const
{
	const FScrubRangeToScreen ScrubRangeToScreen(TimeSliderArgs.ViewRange.Get(), Geometry.Size);
	return ComputeFrameTimeFromMouse(Geometry, ScreenSpacePosition, ScrubRangeToScreen);
}

int32 FMotionTimeSliderController::OnPaintTimeSlider(bool bMirrorLabels, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const bool bEnabled = bParentEnabled;
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	TRange<double> LocalViewRange = TimeSliderArgs.ViewRange.Get();
	const float    LocalViewRangeMin = LocalViewRange.GetLowerBoundValue();
	const float    LocalViewRangeMax = LocalViewRange.GetUpperBoundValue();
	const float    LocalSequenceLength = LocalViewRangeMax - LocalViewRangeMin;
	TRange<FFrameNumber> LocalPlaybackRange = TimeSliderArgs.PlaybackRange.Get();

	FVector2D Scale = FVector2D(1.0f, 1.0f);
	if (LocalSequenceLength > 0.0f)
	{
		FScrubRangeToScreen RangeToScreen(LocalViewRange, AllottedGeometry.Size);

		//Draw Tick Marks
		const float MajorTickHeight = 9.0f;

		FDrawTickArgs Args;
		{
			Args.AllottedGeometry = AllottedGeometry;
			Args.bMirrorLabels = bMirrorLabels;
			Args.bOnlyDrawMajorTicks = false;
			Args.TickColor = FLinearColor::White;
			Args.CullingRect = MyCullingRect;
			Args.DrawEffects = DrawEffects;
			Args.StartLayer = LayerId;
			Args.TickOffset = bMirrorLabels ? 0.0f : FMath::Abs(AllottedGeometry.Size.Y - MajorTickHeight);
			Args.MajorTickHeight = MajorTickHeight;
		}

		DrawTicks(OutDrawElements, LocalViewRange, RangeToScreen, Args);

		//Draw playback & Selection range
		FPaintPlaybackRangeArgs PlaybackRangeArgs(
			bMirrorLabels ? FAppStyle::GetBrush("Sequencer.Timeline.PlayRange_Bottom_L")
			: FAppStyle::GetBrush("Sequencer.Timeline.PlayRange_Top_L"),
			bMirrorLabels ? FAppStyle::GetBrush("Sequencer.Timeline.PlayRange_Bottom_R")
			: FAppStyle::GetBrush("Sequencer.Timeline.PlayRange_Top_R"),
			6.f
		);

		LayerId = DrawPlaybackRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PlaybackRangeArgs);
		PlaybackRangeArgs.SolidFillOpacity = 0.05f;
		LayerId = DrawSelectionRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PlaybackRangeArgs);
		LayerId = DrawEditableTimes(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen);

		//Draw the scrub handle
		const float      HandleStart = RangeToScreen.InputToLocalX(TimeSliderArgs.ScrubPosition.Get().AsDecimal() / GetTickResolution().AsDecimal()) - 7.0f;
		const float      HandleEnd = HandleStart + 13.0f;

		const int32 ArrowLayer = LayerId + 2;
		FPaintGeometry MyGeometry = AllottedGeometry.ToPaintGeometry(FVector2D(HandleEnd - HandleStart,
			AllottedGeometry.Size.Y), FSlateLayoutTransform(1.0f, FVector2D(HandleStart, 0)));
		FLinearColor ScrubColor = InWidgetStyle.GetColorAndOpacityTint();
		{
			ScrubColor.A = ScrubColor.A * 0.75f;
			ScrubColor.B *= 0.1f;
			ScrubColor.G *= 0.2f;
		}

		const FSlateBrush* Brush = (bMirrorLabels ? ScrubHandleUpBrush : ScrubHandleDownBrush);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			ArrowLayer,
			MyGeometry,
			Brush,
			DrawEffects,
			ScrubColor
		);
		{
			//Draw current time next to the scrub handle
			FString FrameString = TimeSliderArgs.NumericTypeInterface->ToString(TimeSliderArgs.ScrubPosition.Get().GetFrame().Value);
			
			FSlateFontInfo SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);

			const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			FVector2D TextSize = FontMeasureService->Measure(FrameString, SmallLayoutFont);

			//Flip the text position if getting near the end of the view range
			static const float TextOffsetPx = 2.0f;
			bool  bDrawLeft = (AllottedGeometry.Size.X - HandleEnd) < (TextSize.X + 14.f) - TextOffsetPx;
			float TextPosition = bDrawLeft ? HandleStart - TextSize.X - TextOffsetPx : HandleEnd + TextOffsetPx;

			FVector2D TextOffset(TextPosition, Args.bMirrorLabels ? TextSize.Y - 6.f : Args.AllottedGeometry.Size.Y - (Args.MajorTickHeight + TextSize.Y));

			FSlateDrawElement::MakeText(
				OutDrawElements,
				Args.StartLayer + 1,
				Args.AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(1.0f, TextOffset)),
				FrameString,
				SmallLayoutFont,
				Args.DrawEffects,
				Args.TickColor
			);
		}

		if (MouseDragType == DRAG_SETTING_RANGE)
		{
			FFrameRate Resolution = GetTickResolution();
			FFrameTime MouseDownTime[2];

			FScrubRangeToScreen MouseDownRange(TimeSliderArgs.ViewRange.Get(), MouseDownGeometry.Size);
			MouseDownTime[0] = ComputeFrameTimeFromMouse(MouseDownGeometry, MouseDownPosition[0], MouseDownRange);
			MouseDownTime[1] = ComputeFrameTimeFromMouse(MouseDownGeometry, MouseDownPosition[1], MouseDownRange);

			float      MouseStartPosX = RangeToScreen.InputToLocalX(MouseDownTime[0] / Resolution);
			float      MouseEndPosX = RangeToScreen.InputToLocalX(MouseDownTime[1] / Resolution);

			float RangePosX = MouseStartPosX < MouseEndPosX ? MouseStartPosX : MouseEndPosX;
			float RangeSizeX = FMath::Abs(MouseStartPosX - MouseEndPosX);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(FVector2D(RangeSizeX, AllottedGeometry.Size.Y), FSlateLayoutTransform(1.0f, FVector2D(RangePosX, 0.f))),
				bMirrorLabels ? ScrubHandleDownBrush : ScrubHandleUpBrush,
				DrawEffects,
				MouseStartPosX < MouseEndPosX ? FLinearColor(0.5f, 0.5f, 0.5f) : FLinearColor(0.25f, 0.3f, 0.3f)
			);
		}

		return ArrowLayer;
	}

	return LayerId;
}



int32 FMotionTimeSliderController::OnPaintViewArea(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bEnabled, const FPaintViewAreaArgs& Args) const
{
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	
	const TRange<double> LocalViewRange = TimeSliderArgs.ViewRange.Get();
	const FScrubRangeToScreen RangeToScreen(LocalViewRange, AllottedGeometry.Size);

	if (Args.PlaybackRangeArgs.IsSet())
	{
		FPaintPlaybackRangeArgs PaintArgs = Args.PlaybackRangeArgs.GetValue();
		LayerId = DrawPlaybackRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PaintArgs);
		PaintArgs.SolidFillOpacity = 0.2f;
		LayerId = DrawSelectionRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PaintArgs);
	}

	if (Args.bDisplayTickLines)
	{
		static FLinearColor TickColor(0.1f, 0.1f, 0.1f, 0.3f);

		// Draw major tick lines in the section area
		FDrawTickArgs DrawTickArgs;
		{
			DrawTickArgs.AllottedGeometry = AllottedGeometry;
			DrawTickArgs.bMirrorLabels = false;
			DrawTickArgs.bOnlyDrawMajorTicks = true;
			DrawTickArgs.TickColor = TickColor;
			DrawTickArgs.CullingRect = MyCullingRect;
			DrawTickArgs.DrawEffects = DrawEffects;
			// Draw major ticks under sections
			DrawTickArgs.StartLayer = LayerId - 1;
			// Draw the tick the entire height of the section area
			DrawTickArgs.TickOffset = 0.0f;
			DrawTickArgs.MajorTickHeight = AllottedGeometry.Size.Y;
		}

		DrawTicks(OutDrawElements, LocalViewRange, RangeToScreen, DrawTickArgs);
	}

	if (Args.bDisplayScrubPosition)
	{
		// Draw a line for the scrub position
		const float LinePos = RangeToScreen.InputToLocalX(TimeSliderArgs.ScrubPosition.Get().AsDecimal() / GetTickResolution().AsDecimal());

		TArray<FVector2D> LinePoints;
		{
			LinePoints.AddUninitialized(2);
			LinePoints[0] = FVector2D(0.0f, 0.0f);
			LinePoints[1] = FVector2D(0.0f, FMath::FloorToFloat(AllottedGeometry.Size.Y));
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(1.0f, 1.0f), FSlateLayoutTransform(1.0f, FVector2D(LinePos, 0.0f))),
			LinePoints,
			DrawEffects,
			FLinearColor(1.f, 1.f, 1.f, .5f),
			false
		);
	}

	TSharedPtr<FMotionModel> MotionModel = WeakModel.Pin();
	if (MotionModel.IsValid())
	{
		const FLinearColor LineColor = GetDefault<UPersonaOptions>()->SectionTimingNodeColor;

		// Draw all the times that we can drag in the timeline
		for (const double Time : MotionModel->GetEditableTimes())
		{
			const float LinePos = RangeToScreen.InputToLocalX(Time);

			TArray<FVector2D> LinePoints;
			{
				LinePoints.AddUninitialized(2);
				LinePoints[0] = FVector2D(0.0f, 0.0f);
				LinePoints[1] = FVector2D(0.0f, FMath::FloorToFloat(AllottedGeometry.Size.Y));
			}

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(FVector2D(1.0f, 1.0f), FSlateLayoutTransform(1.0f, FVector2D(LinePos, 0.0f))),
				LinePoints,
				DrawEffects,
				LineColor,
				false
			);
		}
	}

	return LayerId;
}



FReply FMotionTimeSliderController::OnMouseButtonDown(SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	DistanceDragged = 0;
	MouseDownPosition[0] = MouseDownPosition[1] = MouseEvent.GetScreenSpacePosition();
	MouseDownGeometry = MyGeometry;
	return FReply::Unhandled();
}



FReply FMotionTimeSliderController::OnMouseButtonUp(SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const bool bHandleLeftMouseButton = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && WidgetOwner.HasMouseCapture();
	const bool bHandleRightMouseButton = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && WidgetOwner.HasMouseCapture() && TimeSliderArgs.AllowZoom;

	const FScrubRangeToScreen RangeToScreen = FScrubRangeToScreen(TimeSliderArgs.ViewRange.Get(), MyGeometry.Size);
	const FFrameTime MouseTime = ComputeFrameTimeFromMouse(MyGeometry, MouseEvent.GetScreenSpacePosition(), RangeToScreen);

	if (bHandleRightMouseButton)
	{
		if (!bPanning && DistanceDragged == 0.0f)
		{
			return WeakTimeline.Pin()->OnMouseButtonUp(MyGeometry, MouseEvent).ReleaseMouseCapture();
		}

		bPanning = false;
		DistanceDragged = 0.f;

		return FReply::Handled().ReleaseMouseCapture();
	}
	else if (bHandleLeftMouseButton)
	{
		if (MouseDragType == DRAG_PLAYBACK_START)
		{
			TimeSliderArgs.OnPlaybackRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_PLAYBACK_END)
		{
			TimeSliderArgs.OnPlaybackRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_SELECTION_START)
		{
			TimeSliderArgs.OnSelectionRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_SELECTION_END)
		{
			TimeSliderArgs.OnSelectionRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_SETTING_RANGE)
		{
			// Zooming
			FFrameTime MouseDownStart = ComputeFrameTimeFromMouse(MyGeometry, MouseDownPosition[0], RangeToScreen);

			const bool bCanZoomIn = MouseTime > MouseDownStart;
			const bool bCanZoomOut = ViewRangeStack.Num() > 0;
			if (bCanZoomIn || bCanZoomOut)
			{
				TRange<double> ViewRange = TimeSliderArgs.ViewRange.Get();
				if (!bCanZoomIn)
				{
					ViewRange = ViewRangeStack.Pop();
				}

				if (bCanZoomIn)
				{
					// push the current value onto the stack
					ViewRangeStack.Add(ViewRange);

					ViewRange = TRange<double>(MouseDownStart.FrameNumber / GetTickResolution(), MouseTime.FrameNumber / GetTickResolution());
				}

				TimeSliderArgs.OnViewRangeChanged.ExecuteIfBound(ViewRange, EViewRangeInterpolation::Immediate);
				if (!TimeSliderArgs.ViewRange.IsBound())
				{
					// The output is not bound to a delegate so we'll manage the value ourselves
					TimeSliderArgs.ViewRange.Set(ViewRange);
				}
			}
		}
		else if (MouseDragType == DRAG_TIME)
		{
			double Time = (double)MouseTime.AsDecimal() / GetTickResolution().AsDecimal();

			if (!MouseEvent.IsControlDown())
			{
				const double SnapMargin = (MotionScrubConstants::SnapMarginInPixels / (double)RangeToScreen.PixelsPerInput);
				WeakModel.Pin()->Snap(Time, SnapMargin, { FName("MontageSection") });
			}

			SetEditableTime(DraggedTimeIndex, Time, false);
			DraggedTimeIndex = INDEX_NONE;
		}
		else
		{
			TimeSliderArgs.OnEndScrubberMovement.ExecuteIfBound();

			CommitScrubPosition(MouseTime, /*bIsScrubbing=*/false);
		}

		MouseDragType = DRAG_NONE;
		DistanceDragged = 0.f;

		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

FReply FMotionTimeSliderController::OnMouseMove(SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const bool bHandleLeftMouseButton = MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);
	const bool bHandleRightMouseButton = MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton) && TimeSliderArgs.AllowZoom;

	if (bHandleRightMouseButton)
	{
		if (!bPanning)
		{
			DistanceDragged += FMath::Abs(MouseEvent.GetCursorDelta().X);
			if (DistanceDragged > FSlateApplication::Get().GetDragTriggerDistance())
			{
				bPanning = true;
			}
		}
		else
		{
			const TRange<double> LocalViewRange = TimeSliderArgs.ViewRange.Get();
			const double LocalViewRangeMin = LocalViewRange.GetLowerBoundValue();
			const double LocalViewRangeMax = LocalViewRange.GetUpperBoundValue();

			const FScrubRangeToScreen ScaleInfo(LocalViewRange, MyGeometry.Size);
			const FVector2D ScreenDelta = MouseEvent.GetCursorDelta();
			FVector2D InputDelta;
			InputDelta.X = ScreenDelta.X / ScaleInfo.PixelsPerInput;

			double NewViewOutputMin = LocalViewRangeMin - InputDelta.X;
			double NewViewOutputMax = LocalViewRangeMax - InputDelta.X;

			ClampViewRange(NewViewOutputMin, NewViewOutputMax);
			SetViewRange(NewViewOutputMin, NewViewOutputMax, EViewRangeInterpolation::Immediate);
		}
	}
	else if (bHandleLeftMouseButton)
	{
		const TRange<double> LocalViewRange = TimeSliderArgs.ViewRange.Get();
		const FScrubRangeToScreen RangeToScreen(LocalViewRange, MyGeometry.Size);
		DistanceDragged += FMath::Abs(MouseEvent.GetCursorDelta().X);

		if (MouseDragType == DRAG_NONE)
		{
			if (DistanceDragged > FSlateApplication::Get().GetDragTriggerDistance())
			{
				UAnimMontage* AnimMontage = Cast<UAnimMontage>(WeakModel.Pin()->GetAnimAsset());
				const bool bChildAnimMontage = AnimMontage && AnimMontage->HasParentAsset();

				const FFrameTime MouseDownFree = ComputeFrameTimeFromMouse(MyGeometry, MouseDownPosition[0], RangeToScreen, false);

				const FFrameRate FrameResolution = GetTickResolution();
				const bool       bLockedPlayRange = TimeSliderArgs.IsPlaybackRangeLocked.Get();
				const float      MouseDownPixel = RangeToScreen.InputToLocalX(MouseDownFree / FrameResolution);
				const bool       bHitScrubber = GetHitTestScrubberPixelRange(TimeSliderArgs.ScrubPosition.Get(), RangeToScreen).HandleRange.Contains(MouseDownPixel);
				const int32      HitTimeIndex = HitTestTimes(RangeToScreen, MouseDownPixel);
				const bool       bHitTime = !bChildAnimMontage && HitTimeIndex != INDEX_NONE;

				const TRange<double>   SelectionRange = TimeSliderArgs.SelectionRange.Get() / FrameResolution;
				const TRange<double>   PlaybackRange = TimeSliderArgs.PlaybackRange.Get() / FrameResolution;

				// Disable selection range test if it's empty so that the playback range scrubbing gets priority
				if (!SelectionRange.IsEmpty() && !bHitScrubber && HitTestRangeEnd(RangeToScreen, SelectionRange, MouseDownPixel))
				{
					// selection range end scrubber
					MouseDragType = DRAG_SELECTION_END;
					TimeSliderArgs.OnSelectionRangeBeginDrag.ExecuteIfBound();
				}
				else if (!SelectionRange.IsEmpty() && !bHitScrubber && HitTestRangeStart(RangeToScreen, SelectionRange, MouseDownPixel))
				{
					// selection range start scrubber
					MouseDragType = DRAG_SELECTION_START;
					TimeSliderArgs.OnSelectionRangeBeginDrag.ExecuteIfBound();
				}
				else if (!bLockedPlayRange && !bHitScrubber && HitTestRangeEnd(RangeToScreen, PlaybackRange, MouseDownPixel))
				{
					// playback range end scrubber
					MouseDragType = DRAG_PLAYBACK_END;
					TimeSliderArgs.OnPlaybackRangeBeginDrag.ExecuteIfBound();
				}
				else if (!bLockedPlayRange && !bHitScrubber && HitTestRangeStart(RangeToScreen, PlaybackRange, MouseDownPixel))
				{
					// playback range start scrubber
					MouseDragType = DRAG_PLAYBACK_START;
					TimeSliderArgs.OnPlaybackRangeBeginDrag.ExecuteIfBound();
				}
				else if (FSlateApplication::Get().GetModifierKeys().AreModifersDown(EModifierKey::Control))
				{
					MouseDragType = DRAG_SETTING_RANGE;
				}
				else if (bHitTime)
				{
					MouseDragType = DRAG_TIME;
					DraggedTimeIndex = HitTimeIndex;
				}
				else
				{
					MouseDragType = DRAG_SCRUBBING_TIME;
					TimeSliderArgs.OnBeginScrubberMovement.ExecuteIfBound();
				}
			}
		}
		else
		{
			FFrameTime MouseTime = ComputeFrameTimeFromMouse(MyGeometry, MouseEvent.GetScreenSpacePosition(), RangeToScreen);

			// Set the start range time?
			if (MouseDragType == DRAG_PLAYBACK_START)
			{
				SetPlaybackRangeStart(MouseTime.FrameNumber);
			}
			// Set the end range time?
			else if (MouseDragType == DRAG_PLAYBACK_END)
			{
				SetPlaybackRangeEnd(MouseTime.FrameNumber - 1);
			}
			else if (MouseDragType == DRAG_SELECTION_START)
			{
				SetSelectionRangeStart(MouseTime.FrameNumber);
			}
			// Set the end range time?
			else if (MouseDragType == DRAG_SELECTION_END)
			{
				SetSelectionRangeEnd(MouseTime.FrameNumber);
			}
			else if (MouseDragType == DRAG_SCRUBBING_TIME)
			{
				// Delegate responsibility for clamping to the current viewrange to the client
				CommitScrubPosition(MouseTime, /*bIsScrubbing=*/true);
			}
			else if (MouseDragType == DRAG_SETTING_RANGE)
			{
				MouseDownPosition[1] = MouseEvent.GetScreenSpacePosition();
			}
			else if (MouseDragType == DRAG_TIME)
			{
				double Time = (double)MouseTime.AsDecimal() / GetTickResolution().AsDecimal();

				if (!MouseEvent.IsControlDown())
				{
					const double SnapMargin = (MotionScrubConstants::SnapMarginInPixels / (double)RangeToScreen.PixelsPerInput);
					WeakModel.Pin()->Snap(Time, SnapMargin, { FName("MontageSection") });
				}

				SetEditableTime(DraggedTimeIndex, Time, true);
			}
		}
	}

	if (DistanceDragged != 0.f && (bHandleLeftMouseButton || bHandleRightMouseButton))
	{
		return FReply::Handled().CaptureMouse(WidgetOwner.AsShared());
	}


	return FReply::Handled();
}

FReply FMotionTimeSliderController::OnMouseWheel(SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TOptional<TRange<float>> NewTargetRange;

	if (TimeSliderArgs.AllowZoom && MouseEvent.IsControlDown())
	{
		const float MouseFractionX = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X / MyGeometry.GetLocalSize().X;

		const float ZoomDelta = -0.2f * MouseEvent.GetWheelDelta();
		if (ZoomByDelta(ZoomDelta, MouseFractionX))
		{
			return FReply::Handled();
		}
	}
	else if (MouseEvent.IsShiftDown())
	{
		PanByDelta(-MouseEvent.GetWheelDelta());
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FCursorReply FMotionTimeSliderController::OnCursorQuery(TSharedRef<const SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	const FScrubRangeToScreen RangeToScreen(TimeSliderArgs.ViewRange.Get(), MyGeometry.Size);

	UAnimMontage* AnimMontage = Cast<UAnimMontage>(WeakModel.Pin()->GetAnimAsset());
	const bool bChildAnimMontage = AnimMontage && AnimMontage->HasParentAsset();

	const FFrameRate FrameResolution = GetTickResolution();
	const bool       bLockedPlayRange = TimeSliderArgs.IsPlaybackRangeLocked.Get();
	const float      HitTestPixel = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition()).X;
	const bool       bHitScrubber = GetHitTestScrubberPixelRange(TimeSliderArgs.ScrubPosition.Get(), RangeToScreen).HandleRange.Contains(HitTestPixel);
	const bool       bHitTime = !bChildAnimMontage && (HitTestTimes(RangeToScreen, HitTestPixel) != INDEX_NONE);

	const TRange<double>   SelectionRange = TimeSliderArgs.SelectionRange.Get() / FrameResolution;
	const TRange<double>   PlaybackRange = TimeSliderArgs.PlaybackRange.Get() / FrameResolution;

	if (MouseDragType == DRAG_SCRUBBING_TIME)
	{
		return FCursorReply::Unhandled();
	}

	// Use L/R resize cursor if we're dragging or hovering a playback range bound
	if (bHitTime ||
		(MouseDragType == DRAG_PLAYBACK_END) ||
		(MouseDragType == DRAG_PLAYBACK_START) ||
		(MouseDragType == DRAG_SELECTION_START) ||
		(MouseDragType == DRAG_SELECTION_END) ||
		(MouseDragType == DRAG_TIME) ||
		(!bLockedPlayRange && !bHitScrubber && HitTestRangeStart(RangeToScreen, PlaybackRange, HitTestPixel)) ||
		(!bLockedPlayRange && !bHitScrubber && HitTestRangeEnd(RangeToScreen, PlaybackRange, HitTestPixel)) ||
		(!SelectionRange.IsEmpty() && !bHitScrubber && HitTestRangeStart(RangeToScreen, SelectionRange, HitTestPixel)) ||
		(!SelectionRange.IsEmpty() && !bHitScrubber && HitTestRangeEnd(RangeToScreen, SelectionRange, HitTestPixel)))
	{
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	}

	return FCursorReply::Unhandled();
}

void FMotionTimeSliderController::SetViewRange(double NewRangeMin, double NewRangeMax, EViewRangeInterpolation Interpolation)
{
	// Clamp to a minimum size to avoid zero-sized or negative visible ranges
	const double MinVisibleTimeRange = FFrameNumber(1) / GetTickResolution();
	const TRange<double> ExistingViewRange = TimeSliderArgs.ViewRange.Get();

	if (NewRangeMax == ExistingViewRange.GetUpperBoundValue())
	{
		if (NewRangeMin > NewRangeMax - MinVisibleTimeRange)
		{
			NewRangeMin = NewRangeMax - MinVisibleTimeRange;
		}
	}
	else if (NewRangeMax < NewRangeMin + MinVisibleTimeRange)
	{
		NewRangeMax = NewRangeMin + MinVisibleTimeRange;
	}

	// Clamp to the clamp range
	const TRange<double> NewRange = TRange<double>(NewRangeMin, NewRangeMax);
	TimeSliderArgs.OnViewRangeChanged.ExecuteIfBound(NewRange, Interpolation);

	if (!TimeSliderArgs.ViewRange.IsBound())
	{
		// The  output is not bound to a delegate so we'll manage the value ourselves (no animation)
		TimeSliderArgs.ViewRange.Set(NewRange);
	}
}



void FMotionTimeSliderController::SetClampRange(double NewRangeMin, double NewRangeMax)
{
	const TRange<double> NewRange(NewRangeMin, NewRangeMax);

	TimeSliderArgs.OnClampRangeChanged.ExecuteIfBound(NewRange);

	if (!TimeSliderArgs.ClampRange.IsBound())
	{
		// The  output is not bound to a delegate so we'll manage the value ourselves (no animation)
		TimeSliderArgs.ClampRange.Set(NewRange);
	}
}



void FMotionTimeSliderController::SetPlayRange(FFrameNumber RangeStart, int32 RangeDuration)
{
	check(RangeDuration >= 0);

	const TRange<FFrameNumber> NewRange(RangeStart, RangeStart + RangeDuration);

	TimeSliderArgs.OnPlaybackRangeChanged.ExecuteIfBound(NewRange);

	if (!TimeSliderArgs.PlaybackRange.IsBound())
	{
		// The  output is not bound to a delegate so we'll manage the value ourselves (no animation)
		TimeSliderArgs.PlaybackRange.Set(NewRange);
	}
}


void FMotionTimeSliderController::CommitScrubPosition(FFrameTime NewValue, bool bIsScrubbing)
{
	// Manage the scrub position ourselves if its not bound to a delegate
	if (!TimeSliderArgs.ScrubPosition.IsBound())
	{
		TimeSliderArgs.ScrubPosition.Set(NewValue);
	}
	
	TimeSliderArgs.OnScrubPositionChanged.ExecuteIfBound(NewValue, bIsScrubbing, /*bEvaluate*/true);
}



void FMotionTimeSliderController::DrawTicks(FSlateWindowElementList& OutDrawElements, const TRange<double>& ViewRange, 
	const FScrubRangeToScreen& RangeToScreen, FDrawTickArgs& InArgs) const
{
	const TSharedPtr<SMotionTimeline> Timeline = WeakTimeline.Pin();
	if (!Timeline.IsValid())
	{
		return;
	}

	if (!FMath::IsFinite(ViewRange.GetLowerBoundValue()) || !FMath::IsFinite(ViewRange.GetUpperBoundValue()))
	{
		return;
	}

	const FFrameRate FrameResolution = GetTickResolution();
	const FPaintGeometry PaintGeometry = InArgs.AllottedGeometry.ToPaintGeometry();
	const FSlateFontInfo SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);

	double MajorGridStep = 0.0;
	int32  MinorDivisions = 0;
	if (!Timeline->GetGridMetrics(InArgs.AllottedGeometry.Size.X, MajorGridStep, MinorDivisions))
	{
		return;
	}

	if (InArgs.bOnlyDrawMajorTicks)
	{
		MinorDivisions = 0;
	}

	TArray<FVector2D> LinePoints;
	LinePoints.SetNumUninitialized(2);

	const bool bAntiAliasLines = false;

	const double FirstMajorLine = FMath::FloorToDouble(ViewRange.GetLowerBoundValue() / MajorGridStep) * MajorGridStep;
	const double LastMajorLine = FMath::CeilToDouble(ViewRange.GetUpperBoundValue() / MajorGridStep) * MajorGridStep;

	for (double CurrentMajorLine = FirstMajorLine; CurrentMajorLine < LastMajorLine; CurrentMajorLine += MajorGridStep)
	{
		const float MajorLinePx = RangeToScreen.InputToLocalX(CurrentMajorLine);

		LinePoints[0] = FVector2D(MajorLinePx, InArgs.TickOffset);
		LinePoints[1] = FVector2D(MajorLinePx, InArgs.TickOffset + InArgs.MajorTickHeight);

		// Draw each tick mark
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			InArgs.StartLayer,
			PaintGeometry,
			LinePoints,
			InArgs.DrawEffects,
			InArgs.TickColor,
			bAntiAliasLines
		);

		if (!InArgs.bOnlyDrawMajorTicks)
		{
			FString FrameString = TimeSliderArgs.NumericTypeInterface->ToString((CurrentMajorLine * FrameResolution).RoundToFrame().Value);

			// Space the text between the tick mark but slightly above
			FVector2D TextOffset(MajorLinePx + 5.f, InArgs.bMirrorLabels ? 3.f : FMath::Abs(InArgs.AllottedGeometry.Size.Y - (InArgs.MajorTickHeight + 3.f)));
			FSlateDrawElement::MakeText(
				OutDrawElements,
				InArgs.StartLayer + 1,
				InArgs.AllottedGeometry.ToPaintGeometry(InArgs.AllottedGeometry.Size, FSlateLayoutTransform(1.0f, TextOffset)),
				FrameString,
				SmallLayoutFont,
				InArgs.DrawEffects,
				InArgs.TickColor * 0.65f
			);
		}

		for (int32 Step = 1; Step < MinorDivisions; ++Step)
		{
			// Compute the size of each tick mark.  If we are half way between to visible values display a slightly larger tick mark
			const float MinorTickHeight = ((MinorDivisions % 2 == 0) && (Step % (MinorDivisions / 2)) == 0) ? 6.0f : 2.0f;
			const float MinorLinePx = RangeToScreen.InputToLocalX(CurrentMajorLine + Step * MajorGridStep / MinorDivisions);

			LinePoints[0] = FVector2D(MinorLinePx, InArgs.bMirrorLabels ? 0.0f : FMath::Abs(InArgs.AllottedGeometry.Size.Y - MinorTickHeight));
			LinePoints[1] = FVector2D(MinorLinePx, LinePoints[0].Y + MinorTickHeight);

			// Draw each sub mark
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				InArgs.StartLayer,
				PaintGeometry,
				LinePoints,
				InArgs.DrawEffects,
				InArgs.TickColor,
				bAntiAliasLines
			);
		}
	}
}


int32 FMotionTimeSliderController::DrawSelectionRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, 
	int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const
{
	const TRange<double> SelectionRange = TimeSliderArgs.SelectionRange.Get() / GetTickResolution();

	if (!SelectionRange.IsEmpty() && SelectionRange.HasLowerBound() && SelectionRange.HasUpperBound())
	{
		const float SelectionRangeL = RangeToScreen.InputToLocalX(SelectionRange.GetLowerBoundValue()) - 1;
		const float SelectionRangeR = RangeToScreen.InputToLocalX(SelectionRange.GetUpperBoundValue()) + 1;
		const auto DrawColor = FAppStyle::GetSlateColor("SelectionColor").GetColor(FWidgetStyle());

		if (Args.SolidFillOpacity > 0.f)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(FVector2D(SelectionRangeR - SelectionRangeL,
					AllottedGeometry.Size.Y), FSlateLayoutTransform(1.0f, FVector2D(SelectionRangeL, 0.f))),
				FAppStyle::GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				DrawColor.CopyWithNewOpacity(Args.SolidFillOpacity)
			);
		}

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y),
				FSlateLayoutTransform(1.0f, FVector2D(SelectionRangeL, 0.f))),
			Args.StartBrush,
			ESlateDrawEffect::None,
			DrawColor
		);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y),
				FSlateLayoutTransform(FVector2D(SelectionRangeR - Args.BrushWidth, 0.f))),
			Args.EndBrush,
			ESlateDrawEffect::None,
			DrawColor
		);
	}

	return LayerId + 1;
}



int32 FMotionTimeSliderController::DrawPlaybackRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen,
	const FPaintPlaybackRangeArgs& Args) const
{
	if (!TimeSliderArgs.PlaybackRange.IsSet())
	{
		return LayerId;
	}

	const uint8 OpacityBlend = TimeSliderArgs.SubSequenceRange.Get().IsSet() ? 128 : 255;

	const TRange<FFrameNumber> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();
	const FFrameRate TickResolution = GetTickResolution();
	const float PlaybackRangeL = RangeToScreen.InputToLocalX(PlaybackRange.GetLowerBoundValue() / TickResolution);
	const float PlaybackRangeR = RangeToScreen.InputToLocalX(PlaybackRange.GetUpperBoundValue() / TickResolution) - 1;

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y),
			FSlateLayoutTransform(1.0f, FVector2D(PlaybackRangeL, 0.f))),
		Args.StartBrush,
		ESlateDrawEffect::None,
		FColor(32, 128, 32, OpacityBlend)	// 120, 75, 50 (HSV)
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(FVector2D(Args.BrushWidth, AllottedGeometry.Size.Y),
			FSlateLayoutTransform(1.0f, FVector2D(PlaybackRangeR - Args.BrushWidth, 0.f))),
		Args.EndBrush,
		ESlateDrawEffect::None,
		FColor(128, 32, 32, OpacityBlend)	// 0, 75, 50 (HSV)
	);

	// Black tint for excluded regions
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(FVector2D(PlaybackRangeL, AllottedGeometry.Size.Y),
			FSlateLayoutTransform(1.0f,FVector2D(0.f, 0.f))),
		FAppStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f * OpacityBlend / 255.f)
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(FVector2D(AllottedGeometry.Size.X - PlaybackRangeR,
			AllottedGeometry.Size.Y), FSlateLayoutTransform(1.0f, FVector2D(PlaybackRangeR, 0.f))),
		FAppStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f * OpacityBlend / 255.f)
	);

	return LayerId + 1;
}



bool FMotionTimeSliderController::HitTestRangeStart(const FScrubRangeToScreen& RangeToScreen, const TRange<double>& Range, float HitPixel) const
{
	if (Range.HasLowerBound())
	{
		static float BrushSizeInStateUnits = 6.f, DragToleranceSlateUnits = 2.f, MouseTolerance = 2.f;
		const float  RangeStartPixel = RangeToScreen.InputToLocalX(Range.GetLowerBoundValue());

		// Hit test against the brush region to the right of the playback start position, +/- DragToleranceSlateUnits
		return HitPixel >= RangeStartPixel - MouseTolerance - DragToleranceSlateUnits &&
			HitPixel <= RangeStartPixel + MouseTolerance + BrushSizeInStateUnits + DragToleranceSlateUnits;
	}

	return false;
}



bool FMotionTimeSliderController::HitTestRangeEnd(const FScrubRangeToScreen& RangeToScreen, const TRange<double>& Range, float HitPixel) const
{
	if (Range.HasUpperBound())
	{
		static float BrushSizeInStateUnits = 6.f, DragToleranceSlateUnits = 2.f, MouseTolerance = 2.f;
		const float  RangeEndPixel = RangeToScreen.InputToLocalX(Range.GetUpperBoundValue());

		// Hit test against the brush region to the left of the playback end position, +/- DragToleranceSlateUnits
		return HitPixel >= RangeEndPixel - MouseTolerance - BrushSizeInStateUnits - DragToleranceSlateUnits &&
			HitPixel <= RangeEndPixel + MouseTolerance + DragToleranceSlateUnits;
	}

	return false;
}



bool FMotionTimeSliderController::HitTestTime(const FScrubRangeToScreen& RangeToScreen, double Time, float HitPixel) const
{
	static float HalfBrushSizeInStateUnits = 5.5f, DragToleranceSlateUnits = 2.f, MouseTolerance = 2.f;
	const float  TimePixel = RangeToScreen.InputToLocalX(Time);

	// Hit test against the time position, +/- DragToleranceSlateUnits
	return HitPixel >= TimePixel - MouseTolerance - HalfBrushSizeInStateUnits - DragToleranceSlateUnits &&
		HitPixel <= TimePixel + MouseTolerance + HalfBrushSizeInStateUnits + DragToleranceSlateUnits;
}


int32 FMotionTimeSliderController::HitTestTimes(const FScrubRangeToScreen& RangeToScreen, float HitPixel) const
{
	const TArray<double>& Times = WeakModel.Pin()->GetEditableTimes();
	const int32 NumTimes = Times.Num();
	for (int32 TimeIndex = 0; TimeIndex < NumTimes; ++TimeIndex)
	{
		const double Time = WeakModel.Pin()->GetEditableTimes()[TimeIndex];
		if (HitTestTime(RangeToScreen, Time, HitPixel))
		{
			return TimeIndex;
		}
	}

	return INDEX_NONE;
}

void FMotionTimeSliderController::SetPlaybackRangeStart(FFrameNumber NewStart)
{
	const TRange<FFrameNumber> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();

	if (NewStart <= UE::MovieScene::DiscreteExclusiveUpper(PlaybackRange))
	{
		TimeSliderArgs.OnPlaybackRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(NewStart, PlaybackRange.GetUpperBound()));
	}
}

void FMotionTimeSliderController::SetPlaybackRangeEnd(FFrameNumber NewEnd)
{
	const TRange<FFrameNumber> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();

	if (NewEnd >= UE::MovieScene::DiscreteInclusiveLower(PlaybackRange))
	{
		TimeSliderArgs.OnPlaybackRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(PlaybackRange.GetLowerBound(), NewEnd));
	}
}

void FMotionTimeSliderController::SetSelectionRangeStart(FFrameNumber NewStart)
{
	const TRange<FFrameNumber> SelectionRange = TimeSliderArgs.SelectionRange.Get();

	if (SelectionRange.IsEmpty())
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(NewStart, NewStart + 1));
	}
	else if (NewStart <= UE::MovieScene::DiscreteExclusiveUpper(SelectionRange))
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(NewStart, SelectionRange.GetUpperBound()));
	}
}

void FMotionTimeSliderController::SetSelectionRangeEnd(FFrameNumber NewEnd)
{
	const TRange<FFrameNumber> SelectionRange = TimeSliderArgs.SelectionRange.Get();

	if (SelectionRange.IsEmpty())
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(NewEnd - 1, NewEnd));
	}
	else if (NewEnd >= UE::MovieScene::DiscreteInclusiveLower(SelectionRange))
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(SelectionRange.GetLowerBound(), NewEnd));
	}
}

TSharedRef<SWidget> FMotionTimeSliderController::OpenSetPlaybackRangeMenu(FFrameNumber FrameNumber)
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	const FText CurrentTimeText = FText::FromString(TimeSliderArgs.NumericTypeInterface->ToString(FrameNumber.Value));

	TRange<FFrameNumber> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();

	const TRange<FFrameNumber> SelectionRange = TimeSliderArgs.SelectionRange.Get();
	MenuBuilder.BeginSection("AnimSelectionRangeMenu", FText::Format(LOCTEXT("SelectionRangeTextFormat", "Selection Range ({0}):"), CurrentTimeText));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetSelectionStart", "Set Selection Start"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, FrameNumber] { SetSelectionRangeStart(FrameNumber); }),
				FCanExecuteAction::CreateLambda([FrameNumber, SelectionRange] { return SelectionRange.IsEmpty() || FrameNumber < UE::MovieScene::DiscreteExclusiveUpper(SelectionRange); })
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetSelectionEnd", "Set Selection End"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, FrameNumber] { SetSelectionRangeEnd(FrameNumber); }),
				FCanExecuteAction::CreateLambda([FrameNumber, SelectionRange] { return SelectionRange.IsEmpty() || FrameNumber >= UE::MovieScene::DiscreteInclusiveLower(SelectionRange); })
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ClearSelectionRange", "Clear Selection Range"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, FrameNumber] { TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<FFrameNumber>::Empty()); }),
				FCanExecuteAction::CreateLambda([FrameNumber, SelectionRange] { return !SelectionRange.IsEmpty(); })
			)
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}



FFrameTime FMotionTimeSliderController::ComputeFrameTimeFromMouse(const FGeometry& Geometry, FVector2D ScreenSpacePosition, 
	FScrubRangeToScreen RangeToScreen, bool CheckSnapping /*= true*/) const
{
	const FVector2D CursorPos = Geometry.AbsoluteToLocal(ScreenSpacePosition);
	const double MouseValue = RangeToScreen.LocalXToInput(CursorPos.X);

	return MouseValue * GetTickResolution();
}



FMotionTimeSliderController::FScrubPixelRange FMotionTimeSliderController::GetHitTestScrubberPixelRange(
	FFrameTime ScrubTime, const FScrubRangeToScreen& RangeToScreen) const
{
	static const float DragToleranceSlateUnits = 2.f, MouseTolerance = 2.f;
	return GetScrubberPixelRange(ScrubTime, GetTickResolution(), GetDisplayRate(), RangeToScreen, DragToleranceSlateUnits + MouseTolerance);
}

FMotionTimeSliderController::FScrubPixelRange FMotionTimeSliderController::GetScrubberPixelRange(FFrameTime ScrubTime, const FScrubRangeToScreen& RangeToScreen) const
{
	return GetScrubberPixelRange(ScrubTime, GetTickResolution(), GetDisplayRate(), RangeToScreen);
}

FMotionTimeSliderController::FScrubPixelRange FMotionTimeSliderController::GetScrubberPixelRange(FFrameTime ScrubTime, FFrameRate Resolution, 
	FFrameRate PlayRate, const FScrubRangeToScreen& RangeToScreen, float DilationPixels /*= 0.f*/) const
{
	const FFrameNumber Frame = ScrubTime.FloorToFrame();// ConvertFrameTime(ScrubTime, Resolution, PlayRate).FloorToFrame();

	float StartPixel = RangeToScreen.InputToLocalX(Frame / Resolution);
	float EndPixel = RangeToScreen.InputToLocalX((Frame + 1) / Resolution);

	{
		const float RoundedStartPixel = FMath::RoundToInt(StartPixel);
		EndPixel -= (StartPixel - RoundedStartPixel);

		StartPixel = RoundedStartPixel;
		EndPixel = FMath::Max(EndPixel, StartPixel + 1);
	}

	FScrubPixelRange Range;

	const float MinScrubSize = 14.f;
	Range.bClamped = EndPixel - StartPixel < MinScrubSize;
	Range.Range = TRange<float>(StartPixel, EndPixel);
	if (Range.bClamped)
	{
		Range.HandleRange = TRange<float>(
			(StartPixel + EndPixel - MinScrubSize) * .5f,
			(StartPixel + EndPixel + MinScrubSize) * .5f);
	}
	else
	{
		Range.HandleRange = Range.Range;
	}

	return Range;
}

void FMotionTimeSliderController::SetEditableTime(int32 TimeIndex, float Time, bool bIsDragging)
{
	const TSharedPtr<FMotionModel> Model = WeakModel.Pin();
	Model->SetEditableTime(TimeIndex, Time, bIsDragging);
}

int32 FMotionTimeSliderController::DrawEditableTimes(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen) const
{
	const FLinearColor TimeColor = GetDefault<UPersonaOptions>()->SectionTimingNodeColor;

	// Draw all the times that we can drag in the timeline
	for (const double Time : WeakModel.Pin()->GetEditableTimes())
	{
		const float LinePos = RangeToScreen.InputToLocalX(Time);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(11.0f, 12.0f),
				FSlateLayoutTransform(1.0f, FVector2D(LinePos - 6.0f,
					AllottedGeometry.Size.Y - 12.0f))),
			EditableTimeBrush,
			ESlateDrawEffect::None,
			TimeColor
		);
	}

	return LayerId + 1;
}

#undef LOCTEXT_NAMESPACE