//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "SMotionOutlinerItem.h"
#include "Controls/MotionTimelineTrack.h"
#include "Widgets/Text/STextBlock.h"
#include "SMotionOutliner.h"
#include "Widgets/SOverlay.h"
#include "SMotionTrackResizeArea.h"


#define LOCTEXT_NAMESPACE "SMotionOutlinerItem"

SMotionOutlinerItem::~SMotionOutlinerItem()
{
	TSharedPtr<SMotionOutliner> Outliner = StaticCastSharedPtr<SMotionOutliner>(OwnerTablePtr.Pin());
	TSharedPtr<FMotionTimelineTrack> PinnedTrack = Track.Pin();
	if (Outliner.IsValid() && PinnedTrack.IsValid())
	{
		Outliner->OnChildRowRemoved(PinnedTrack.ToSharedRef());
	}
}

void SMotionOutlinerItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, 
	const TSharedRef<FMotionTimelineTrack>& InTrack)
{
	Track = InTrack;
	OnGenerateWidgetForColumn = InArgs._OnGenerateWidgetForColumn;
	HighlightText = InArgs._HighlightText;

	SMultiColumnTableRow::Construct(
		SMultiColumnTableRow::FArguments()
		.ShowSelection(true), InOwnerTableView);
}

void SMotionOutlinerItem::Tick(const FGeometry& AllottedGeometry, 
	const double InCurrentTime, const float InDeltaTime)
{
	StaticCastSharedPtr<SMotionOutliner>(OwnerTablePtr.Pin())->ReportChildRowGeometry(Track.Pin().ToSharedRef(), AllottedGeometry);
}

FVector2D SMotionOutlinerItem::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	TSharedPtr<FMotionTimelineTrack> PinnedTrack = Track.Pin();
	if (PinnedTrack.IsValid())
	{
		return FVector2D(100.0f, PinnedTrack->GetHeight() + PinnedTrack->GetPadding().Combined());
	}

	return FVector2D(100.0f, 16.0f);
}

void SMotionOutlinerItem::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SMultiColumnTableRow<TSharedRef<FMotionTimelineTrack>>::OnMouseEnter(MyGeometry, MouseEvent);

	TSharedPtr<FMotionTimelineTrack> PinnedTrack = Track.Pin();
	if (PinnedTrack.IsValid())
	{
		PinnedTrack->SetHovered(true);
	}
}

void SMotionOutlinerItem::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SMultiColumnTableRow<TSharedRef<FMotionTimelineTrack>>::OnMouseLeave(MouseEvent);

	TSharedPtr<FMotionTimelineTrack> PinnedTrack = Track.Pin();
	if (PinnedTrack.IsValid())
	{
		PinnedTrack->SetHovered(false);
	}
}

bool SMotionOutlinerItem::IsHovered() const
{
	TSharedPtr<FMotionTimelineTrack> PinnedTrack = Track.Pin();
	if (PinnedTrack.IsValid())
	{
		return SMultiColumnTableRow<TSharedRef<FMotionTimelineTrack>>::IsHovered() || PinnedTrack->IsHovered();
	}

	return SMultiColumnTableRow<TSharedRef<FMotionTimelineTrack>>::IsHovered();
}

TSharedRef<SWidget> SMotionOutlinerItem::GenerateWidgetForColumn(const FName& ColumnId)
{
	TSharedPtr<FMotionTimelineTrack> PinnedTrack = Track.Pin();
	if (PinnedTrack.IsValid())
	{
		TSharedPtr<SWidget> ColumnWidget = SNullWidget::NullWidget;
		if (OnGenerateWidgetForColumn.IsBound())
		{
			ColumnWidget = OnGenerateWidgetForColumn.Execute(PinnedTrack.ToSharedRef(), ColumnId, SharedThis(this));
		}

		return SNew(SOverlay)
			+ SOverlay::Slot()
			[
				ColumnWidget.ToSharedRef()
			];
	}

	return SNullWidget::NullWidget;
}

void SMotionOutlinerItem::AddTrackAreaReference(const TSharedPtr<SMotionTrack>& InTrackWidget)
{
	TrackWidget = InTrackWidget;
}

#undef LOCTEXT_NAMESPACE