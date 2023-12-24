//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "SMotionOutliner.h"
#include "Controls/MotionModel.h"
#include "Controls/MotionTimelineTrack.h"
#include "SMotionOutlinerItem.h"
#include "SMotionTrackArea.h"
#include "Widgets/Input/SButton.h"
#include "SMotionTrack.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/TextFilterExpressionEvaluator.h"

#define LOCTEXT_NAMESPACE "MotionSymphonyEditor"

SMotionOutliner::~SMotionOutliner()
{
	if (WeakModel.IsValid())
	{
		WeakModel.Pin()->OnTracksChanged().Remove(TracksChangedDelegateHandle);
	}
}

void SMotionOutliner::Construct(const FArguments& InArgs, const TSharedRef<FMotionModel>& InMotionModel,
	const TSharedRef<SMotionTrackArea>& InTrackArea)
{
	WeakModel = InMotionModel;
	TrackArea = InTrackArea;
	FilterText = InArgs._FilterText;
	bPhysicalTracksNeedUpdate = false;

	TracksChangedDelegateHandle = InMotionModel->OnTracksChanged().AddSP(this, &SMotionOutliner::HandleTracksChanged);

	TextFilter = MakeShareable(new FTextFilterExpressionEvaluator(ETextFilterExpressionEvaluatorMode::BasicString));

	HeaderRow = SNew(SHeaderRow)
		.Visibility(EVisibility::Collapsed);

	HeaderRow->AddColumn(
		SHeaderRow::Column(TEXT("Outliner"))
		.FillWidth(1.0f)
	);

	STreeView::Construct
	(
		STreeView::FArguments()
		.TreeItemsSource(&InMotionModel->GetRootTracks())
		.SelectionMode(ESelectionMode::Multi)
		.OnGenerateRow(this, &SMotionOutliner::HandleGenerateRow)
		.OnGetChildren(this, &SMotionOutliner::HandleGetChildren)
		.HeaderRow(HeaderRow)
		.ExternalScrollbar(InArgs._ExternalScrollbar)
		.OnExpansionChanged(this, &SMotionOutliner::HandleExpansionChanged)
		.AllowOverscroll(EAllowOverscroll::No)
		.OnContextMenuOpening(this, &SMotionOutliner::HandleContextMenuOpening)
	);

	// expand all
	for (TSharedRef<FMotionTimelineTrack>& RootTrack : InMotionModel->GetRootTracks())
	{
		RootTrack->Traverse_ParentFirst([this](FMotionTimelineTrack& InTrack) 
		{ 
			SetItemExpansion(InTrack.AsShared(), InTrack.IsExpanded()); return true; 
		});
	}
}

void SMotionOutliner::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	STreeView::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// These are updated in both tick and paint since both calls can cause changes to the cached rows and the data needs
	// to be kept synchronized so that external measuring calls get correct and reliable results.
	if (bPhysicalTracksNeedUpdate)
	{
		PhysicalTracks.Reset();
		CachedTrackGeometry.GenerateValueArray(PhysicalTracks);

		PhysicalTracks.Sort([](const FCachedGeometry& A, const FCachedGeometry& B)
			{
				return A.Top < B.Top;
			});

		bPhysicalTracksNeedUpdate = false;
	}
}

int32 SMotionOutliner::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, 
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, 
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = STreeView::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	// These are updated in both tick and paint since both calls can cause changes to the cached rows and the data needs
	// to be kept synchronized so that external measuring calls get correct and reliable results.
	if (bPhysicalTracksNeedUpdate)
	{
		PhysicalTracks.Reset();
		CachedTrackGeometry.GenerateValueArray(PhysicalTracks);

		PhysicalTracks.Sort([](const FCachedGeometry& A, const FCachedGeometry& B)
			{
				return A.Top < B.Top;
			});

		bPhysicalTracksNeedUpdate = false;
	}

	return LayerId + 1;
}

void SMotionOutliner::Private_SetItemSelection(TSharedRef<FMotionTimelineTrack> TheItem, 
	bool bShouldBeSelected, bool bWasUserDirected /*= false*/)
{
	if (TheItem->SupportsSelection())
	{
		WeakModel.Pin()->SetTrackSelected(TheItem, bShouldBeSelected);

		STreeView::Private_SetItemSelection(TheItem, bShouldBeSelected, bWasUserDirected);
	}
}

void SMotionOutliner::Private_ClearSelection()
{
	WeakModel.Pin()->ClearTrackSelection();

	STreeView::Private_ClearSelection();
}

void SMotionOutliner::Private_SelectRangeFromCurrentTo(TSharedRef<FMotionTimelineTrack> InRangeSelectionEnd)
{
	STreeView::Private_SelectRangeFromCurrentTo(InRangeSelectionEnd);

	for (TSet<TSharedRef<FMotionTimelineTrack>>::TIterator Iter = SelectedItems.CreateIterator(); Iter; ++Iter)
	{
		if (!(*Iter)->SupportsSelection())
		{
			Iter.RemoveCurrent();
		}
	}

	for (const TSharedRef<FMotionTimelineTrack>& SelectedItem : SelectedItems)
	{
		WeakModel.Pin()->SetTrackSelected(SelectedItem, true);
	}
}

TOptional<SMotionOutliner::FCachedGeometry> SMotionOutliner::GetCachedGeometryForTrack(
	const TSharedRef<FMotionTimelineTrack>& InTrack) const
{
	if (const FCachedGeometry* FoundGeometry = CachedTrackGeometry.Find(InTrack))
	{
		return *FoundGeometry;
	}

	return TOptional<FCachedGeometry>();
}

TOptional<float> SMotionOutliner::ComputeTrackPosition(const TSharedRef<FMotionTimelineTrack>& InTrack) const
{
	// Positioning strategy:
	// Attempt to root out any visible track in the specified track's sub-hierarchy, and compute the track's offset from that
	float NegativeOffset = 0.f;
	TOptional<float> Top;

	// Iterate parent first until we find a tree view row we can use for the offset height
	auto Iter = [&](FMotionTimelineTrack& InTrack)
	{
		TOptional<FCachedGeometry> ChildRowGeometry = GetCachedGeometryForTrack(InTrack.AsShared());
		if (ChildRowGeometry.IsSet())
		{
			Top = ChildRowGeometry->Top;
			// Stop iterating
			return false;
		}

		NegativeOffset -= InTrack.GetHeight() + InTrack.GetPadding().Combined();
		return true;
	};

	InTrack->TraverseVisible_ParentFirst(Iter);

	if (Top.IsSet())
	{
		return NegativeOffset + Top.GetValue();
	}

	return Top;
}

void SMotionOutliner::ReportChildRowGeometry(const TSharedRef<FMotionTimelineTrack>& InTrack, const FGeometry& InGeometry)
{
	float ChildOffset = TransformPoint(
		Concatenate(
			InGeometry.GetAccumulatedLayoutTransform(),
			GetCachedGeometry().GetAccumulatedLayoutTransform().Inverse()
		),
		FVector2D(0, 0)
	).Y;

	FCachedGeometry* ExistingGeometry = CachedTrackGeometry.Find(InTrack);
	if (ExistingGeometry == nullptr || (ExistingGeometry->Top != ChildOffset || ExistingGeometry->Height != InGeometry.Size.Y))
	{
		CachedTrackGeometry.Add(InTrack, FCachedGeometry(InTrack, ChildOffset, InGeometry.Size.Y));
		bPhysicalTracksNeedUpdate = true;
	}
}

void SMotionOutliner::OnChildRowRemoved(const TSharedRef<FMotionTimelineTrack>& InTrack)
{
	CachedTrackGeometry.Remove(InTrack);
	bPhysicalTracksNeedUpdate = true;
}

void SMotionOutliner::ScrollByDelta(float DeltaInSlateUnits)
{
	ScrollBy(GetCachedGeometry(), DeltaInSlateUnits, EAllowOverscroll::No);
}

void SMotionOutliner::RefreshFilter()
{
	TextFilter->SetFilterText(FilterText.Get());

	RequestTreeRefresh();
}

TSharedRef<ITableRow> SMotionOutliner::HandleGenerateRow(TSharedRef<FMotionTimelineTrack> InTrack, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SMotionOutlinerItem> Row =
		SNew(SMotionOutlinerItem, OwnerTable, InTrack)
		.OnGenerateWidgetForColumn(this, &SMotionOutliner::GenerateWidgetForColumn)
		.HighlightText(FilterText);

	// Ensure the track area is kept up to date with the virtualized scroll of the tree view
	TSharedPtr<SMotionTrack> TrackWidget = TrackArea->FindTrackSlot(InTrack);

	if (!TrackWidget.IsValid())
	{
		// Add a track slot for the row
		TrackWidget = SNew(SMotionTrack, InTrack, SharedThis(this))
			.ViewRange(WeakModel.Pin().Get(), &FMotionModel::GetViewRange)
			[
				InTrack->GenerateContainerWidgetForTimeline()
			];

		TrackArea->AddTrackSlot(InTrack, TrackWidget);
	}

	if (ensure(TrackWidget.IsValid()))
	{
		Row->AddTrackAreaReference(TrackWidget);
	}

	return Row;
}

void SMotionOutliner::HandleGetChildren(TSharedRef<FMotionTimelineTrack> Item, TArray<TSharedRef<FMotionTimelineTrack>>& OutChildren)
{
	if (!FilterText.Get().IsEmpty())
	{
		for (const TSharedRef<FMotionTimelineTrack>& Child : Item->GetChildren())
		{
			if (!Child->SupportsFiltering() || TextFilter->TestTextFilter(FBasicStringFilterExpressionContext(Child->GetLabel().ToString())))
			{
				OutChildren.Add(Child);
			}
		}
	}
	else
	{
		OutChildren.Append(Item->GetChildren());
	}
}

void SMotionOutliner::HandleExpansionChanged(TSharedRef<FMotionTimelineTrack> InTrack, bool bIsExpanded)
{
	InTrack->SetExpanded(bIsExpanded);

	// Expand any children that are also expanded
	for (const TSharedRef<FMotionTimelineTrack>& Child : InTrack->GetChildren())
	{
		if (Child->IsExpanded())
		{
			SetItemExpansion(Child, true);
		}
	}
}

TSharedPtr<SWidget> SMotionOutliner::HandleContextMenuOpening()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, WeakModel.Pin()->GetCommandList());

	WeakModel.Pin()->BuildContextMenu(MenuBuilder);

	// > 1 because the search widget is always added
	return MenuBuilder.GetMultiBox()->GetBlocks().Num() > 1 ? MenuBuilder.MakeWidget() : TSharedPtr<SWidget>();

	return TSharedPtr<SWidget>();
}

void SMotionOutliner::HandleTracksChanged()
{
	RequestTreeRefresh();
}

TSharedRef<SWidget> SMotionOutliner::GenerateWidgetForColumn(const TSharedRef<FMotionTimelineTrack>& InTrack, const FName& ColumnId, const TSharedRef<SMotionOutlinerItem>& Row) const
{
	return InTrack->GenerateContainerWidgetForOutliner(Row);
}

#undef LOCTEXT_NAMESPACE