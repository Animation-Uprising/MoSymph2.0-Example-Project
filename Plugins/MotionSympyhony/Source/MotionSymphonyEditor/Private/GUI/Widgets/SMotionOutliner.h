//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Input/Reply.h"
#include "Styling/ISlateStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Controls/MotionTimelineTrack.h"

class FMotionModel;
class SMotionTrackArea;
class SMotionOutlinerItem;
class FMenuBuilder;
class FTextFilterExpressionEvaluator;

/** A delegate that is executed when adding menu content. */
DECLARE_DELEGATE_OneParam(FOnGetContextMenuContent, FMenuBuilder& /*MenuBuilder*/);

class SMotionOutliner : public STreeView<TSharedRef<FMotionTimelineTrack>>
{
public:
	SLATE_BEGIN_ARGS(SMotionOutliner) {}

	SLATE_ARGUMENT(TSharedPtr<SScrollBar>, ExternalScrollbar)

	SLATE_EVENT(FOnGetContextMenuContent, OnGetContextMenuContent)

	SLATE_ATTRIBUTE(FText, FilterText)

	SLATE_END_ARGS()

public:
	/** Structure used to cache physical geometry for a particular track */
	struct FCachedGeometry
	{
		FCachedGeometry(TSharedRef<FMotionTimelineTrack> InTrack, float InTop, float InHeight)
			: Track(MoveTemp(InTrack))
			, Top(InTop)
			, Height(InHeight)
		{}

		TSharedRef<FMotionTimelineTrack> Track;
		float Top;
		float Height;
	};

public:
	/** The anim timeline model */
	TWeakPtr<FMotionModel> WeakModel;

	/** The track area */
	TSharedPtr<SMotionTrackArea> TrackArea;

	/** The header row */
	TSharedPtr<SHeaderRow> HeaderRow;

	/** Delegate handle for track changes */
	FDelegateHandle TracksChangedDelegateHandle;

	/** Map of cached geometries for visible tracks */
	TMap<TSharedRef<FMotionTimelineTrack>, FCachedGeometry> CachedTrackGeometry;

	/** Linear, sorted array of tracks that we currently have generated widgets for */
	mutable TArray<FCachedGeometry> PhysicalTracks;

	/** A flag indicating that the physical tracks need to be updated. */
	mutable bool bPhysicalTracksNeedUpdate;

	/** The filter text used when we are searching the tree */
	TAttribute<FText> FilterText;

	/** Compiled filter search terms. */
	TSharedPtr<FTextFilterExpressionEvaluator> TextFilter;

public:
	~SMotionOutliner();

	void Construct(const FArguments& InArgs, const TSharedRef<FMotionModel>& InAnimModel, 
		const TSharedRef<SMotionTrackArea>& InTrackArea);

	/** SWidget Interface*/
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, 
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, 
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;

	/** STreeView Interface */
	virtual void Private_SetItemSelection(TSharedRef<FMotionTimelineTrack> TheItem, 
		bool bShouldBeSelected, bool bWasUserDirected = false) override;
	virtual void Private_ClearSelection() override;
	virtual void Private_SelectRangeFromCurrentTo(TSharedRef<FMotionTimelineTrack> InRangeSelectionEnd) override;

	/** Get the cached geometry for the specified track */
	TOptional<FCachedGeometry> GetCachedGeometryForTrack(const TSharedRef<FMotionTimelineTrack>& InTrack) const;

	/** Compute the position of the specified track using its cached geometry */
	TOptional<float> ComputeTrackPosition(const TSharedRef<FMotionTimelineTrack>& InTrack) const;

	/** Report geometry for a child row */
	void ReportChildRowGeometry(const TSharedRef<FMotionTimelineTrack>& InTrack, const FGeometry& InGeometry);

	/** Called when a child row widget has been added/removed */
	void OnChildRowRemoved(const TSharedRef<FMotionTimelineTrack>& InTrack);

	/** Scroll this tree view by the specified number of slate units */
	void ScrollByDelta(float DeltaInSlateUnits);

	/** Apply any changed filter */
	void RefreshFilter();

private:
	/** Generate a row for the outliner */
	TSharedRef<ITableRow> HandleGenerateRow(TSharedRef<FMotionTimelineTrack> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Get the children of an outliner item */
	void HandleGetChildren(TSharedRef<FMotionTimelineTrack> Item, TArray<TSharedRef<FMotionTimelineTrack>>& OutChildren);

	/** Record the expansion state when it changes */
	void HandleExpansionChanged(TSharedRef<FMotionTimelineTrack> InItem, bool bIsExpanded);

	/** Handle context menu */
	TSharedPtr<SWidget> HandleContextMenuOpening();

	/** Handle tracks changing */
	void HandleTracksChanged();

	/** Generate a widget for the specified column */
	TSharedRef<SWidget> GenerateWidgetForColumn(const TSharedRef<FMotionTimelineTrack>& InTrack, 
		const FName& ColumnId, const TSharedRef<SMotionOutlinerItem>& Row) const;

};