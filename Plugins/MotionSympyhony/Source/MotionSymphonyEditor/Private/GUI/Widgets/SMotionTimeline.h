//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "ITimeSlider.h"
#include "Controls/MotionModel.h"
#include "ITransportControl.h"


class SMotionOutliner;
class SMotionTrackArea;
class FMotionTimeSliderController;
class SSearchBox;
struct FPaintPlaybackRangeArgs;
class UAnimSingleNodeInstance;

class SMotionTimeline : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMotionTimeline) {}

	SLATE_ATTRIBUTE(FInt32Interval, ViewIndices)

	SLATE_EVENT(FSimpleDelegate, OnReceivedFocus)

	SLATE_END_ARGS()

private:
	/** Anim timeline model */
	TSharedPtr<FMotionModel> Model;

	TWeakPtr<FUICommandList> WeakCommandList;

	TObjectPtr<UMotionAnimObject> MotionAnim;

	UDebugSkelMeshComponent* DebugSkelMeshComponent;

	/** Outliner widget */
	TSharedPtr<SMotionOutliner> Outliner;

	/** Track area widget */
	TSharedPtr<SMotionTrackArea> TrackArea;

	/** The time slider controller */
	TSharedPtr<FMotionTimeSliderController> TimeSliderController;

	/** The top time slider widget */
	TSharedPtr<ITimeSlider> TopTimeSlider;

	/** The search box for filtering tracks. */
	TSharedPtr<SSearchBox> SearchBox;

	/** The fill coefficients of each column in the grid. */
	float ColumnFillCoefficients[2];

	/** Called when the user has begun dragging the selection selection range */
	FSimpleDelegate OnSelectionRangeBeginDrag;

	/** Called when the user has finished dragging the selection selection range */
	FSimpleDelegate OnSelectionRangeEndDrag;

	/** Called when any widget contained within the anim timeline has received focus */
	FSimpleDelegate OnReceivedFocus;

	/** Numeric Type interface for converting between frame numbers and display formats. */
	TSharedPtr<INumericTypeInterface<double>> NumericTypeInterface;

	/** Secondary numeric Type interface for converting between frame numbers and display formats. */
	TSharedPtr<INumericTypeInterface<double>> SecondaryNumericTypeInterface;

	/** The view range */
	TAttribute<FAnimatedRange> ViewRange;

	/** Filter text used to search the tree */
	FText FilterText;

	TWeakPtr<class FMotionPreProcessToolkit> MotionPreProcessToolkitPtr;

public:
	void Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList, TWeakPtr<class FMotionPreProcessToolkit> InMotionPreProcessToolkitPtr);

	void Rebuild();
	void DestroyTimeline();

	/** SWidget interface */
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** Compute a major grid interval and number of minor divisions to display */
	bool GetGridMetrics(float PhysicalWidth, double& OutMajorInterval, int32& OutMinorDivisions) const;

	/**Get the time slider controller */
	TSharedPtr<ITimeSliderController> GetTimeSliderController() const;

	void SetAnimation(TObjectPtr<UMotionAnimObject> InMotionAnim, UDebugSkelMeshComponent* InDebugMeshComponent);

private:
	float GetColumnFillCoefficient(int32 ColumnIndex) const
	{
		return ColumnFillCoefficients[ColumnIndex];
	}

	/** Get numeric Type interface for converting between frame numbers and display formats. */
	TSharedRef<INumericTypeInterface<double>> GetNumericTypeInterface() const;

	/** Called when the outliner search terms change */
	void OnOutlinerSearchChanged(const FText& Filter);

	/** Called when a column fill percentage is changed by a splitter slot. */
	void OnColumnFillCoefficientChanged(float FillCoefficient, int32 ColumnIndex);

	/** Handles an additive layer key being set */
	void HandleKeyComplete();

	UAnimSingleNodeInstance* GetPreviewInstance() const;
	
	void HandleScrubPositionChanged(FFrameTime NewScrubPosition, bool bIsScrubbing, bool bParam);
	double GetSpinboxDelta() const;
	void SetPlayTime(double InFrameTime);

	void HandleViewRangeChanged(TRange<double> InRange, EViewRangeInterpolation InInterpolation);
	void HandleWorkingRangeChanged(TRange<double> InRange);
};