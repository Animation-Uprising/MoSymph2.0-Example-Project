//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Controls/MotionTimelineTrack.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "EditorStyleSet.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/SMotionOutlinerItem.h"

#define LOCTEXT_NAMESPACE "FMotionTimelineTrack"

const float FMotionTimelineTrack::OutlinerRightPadding = 8.0f;

MOTIONTIMELINE_IMPLEMENT_TRACK(FMotionTimelineTrack);

FText FMotionTimelineTrack::GetLabel() const
{
	return DisplayName;
}

FText FMotionTimelineTrack::GetTooltipText() const
{
	return ToolTipText;
}

bool FMotionTimelineTrack::Traverse_ChildFirst(const TFunctionRef<bool(FMotionTimelineTrack&)>& InPredicate, bool bIncludeThisTrack /*= true*/)
{
	for (TSharedRef<FMotionTimelineTrack>& Child : Children)
	{
		if (!Child->Traverse_ChildFirst(InPredicate, true))
		{
			return false;
		}
	}

	return bIncludeThisTrack ? InPredicate(*this) : true;
}

bool FMotionTimelineTrack::Traverse_ParentFirst(const TFunctionRef<bool(FMotionTimelineTrack&)>& InPredicate, bool bIncludeThisTrack /*= true*/)
{
	if (bIncludeThisTrack && !InPredicate(*this))
	{
		return false;
	}

	for (TSharedRef<FMotionTimelineTrack>& Child : Children)
	{
		if (!Child->Traverse_ParentFirst(InPredicate, true))
		{
			return false;
		}
	}

	return true;
}

bool FMotionTimelineTrack::TraverseVisible_ChildFirst(const TFunctionRef<bool(FMotionTimelineTrack&)>& InPredicate, bool bIncludeThisTrack /*= true*/)
{
	// If the item is not expanded, its children ain't visible
	if (IsExpanded())
	{
		for (TSharedRef<FMotionTimelineTrack>& Child : Children)
		{
			if (Child->IsVisible() && !Child->TraverseVisible_ChildFirst(InPredicate, true))
			{
				return false;
			}
		}
	}

	if (bIncludeThisTrack && IsVisible())
	{
		return InPredicate(*this);
	}

	// Continue iterating regardless of visibility
	return true;
}

bool FMotionTimelineTrack::TraverseVisible_ParentFirst(const TFunctionRef<bool(FMotionTimelineTrack&)>& InPredicate, bool bIncludeThisTrack /*= true*/)
{
	if (bIncludeThisTrack && IsVisible() && !InPredicate(*this))
	{
		return false;
	}

	// If the item is not expanded, its children ain't visible
	if (IsExpanded())
	{
		for (TSharedRef<FMotionTimelineTrack>& Child : Children)
		{
			if (Child->IsVisible() && !Child->TraverseVisible_ParentFirst(InPredicate, true))
			{
				return false;
			}
		}
	}

	return true;
}

TSharedRef<SWidget> FMotionTimelineTrack::GenerateContainerWidgetForOutliner(const TSharedRef<SMotionOutlinerItem>& InRow)
{
	TSharedPtr<SBorder> OuterBorder;
	TSharedPtr<SHorizontalBox> InnerHorizontalBox;
	TSharedRef<SWidget> Widget = GenerateStandardOutlinerWidget(InRow, true, OuterBorder, InnerHorizontalBox);

	if (bIsHeaderTrack)
	{
		OuterBorder->SetBorderBackgroundColor(FAppStyle::GetColor("AnimTimeline.Outliner.HeaderColor"));
	}

	return Widget;
}

TSharedRef<SWidget> FMotionTimelineTrack::GenerateContainerWidgetForTimeline()
{
	return SNullWidget::NullWidget;
}

void FMotionTimelineTrack::AddToContextMenu(FMenuBuilder& InMenuBuilder, TSet<FName>& InOutExistingMenuTypes) const
{

}

TSharedRef<SWidget> FMotionTimelineTrack::GenerateStandardOutlinerWidget(const TSharedRef<SMotionOutlinerItem>& InRow, 
	bool bWithLabelText, TSharedPtr<SBorder>& OutOuterBorder, TSharedPtr<SHorizontalBox>& OutInnerHorizontalBox)
{
	TSharedRef<SWidget> Widget =
		SAssignNew(OutOuterBorder, SBorder)
		.ToolTipText(this, &FMotionTimelineTrack::GetTooltipText)
		.BorderImage(FAppStyle::GetBrush("Sequencer.Section.BagkroundTint"))
		.BorderBackgroundColor(FAppStyle::GetColor("AnimTimeline.Outliner.ItemColor"))
		[
			SAssignNew(OutInnerHorizontalBox, SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(4.0f, 1.0f)
			[
				SNew(SExpanderArrow, InRow)
			]
		];

	if (bWithLabelText)
	{
		OutInnerHorizontalBox->AddSlot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.Padding(2.0f, 1.0f)
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("AnimTimeline.Outliner.Label"))
				.Text(this, &FMotionTimelineTrack::GetLabel)
				.HighlightText(InRow->GetHighlightText())
			];
	}

	return Widget;
}

float FMotionTimelineTrack::GetMaxInput() const
{
	return GetModel()->GetPlayLength();

}

float FMotionTimelineTrack::GetViewMinInput() const
{
	return GetModel()->GetViewRange().GetLowerBoundValue();
}

float FMotionTimelineTrack::GetViewMaxInput() const
{
	return GetModel()->GetViewRange().GetUpperBoundValue();
}

float FMotionTimelineTrack::GetScrubValue() const
{
	const int32 Resolution = FMath::RoundToInt((double)GetDefault<UPersonaOptions>()->TimelineScrubSnapValue * GetModel()->GetFrameRate());
	return (float)((double)GetModel()->GetScrubPosition().Value / (double)Resolution);
}

void FMotionTimelineTrack::SelectObjects(const TArray<UObject*>& SelectedItems)
{
	GetModel()->SelectObjects(SelectedItems);
}

void FMotionTimelineTrack::OnSetInputViewRange(float ViewMin, float ViewMax)
{
	GetModel()->SetViewRange(TRange<double>(ViewMin, ViewMax));
}

#undef LOCTEXT_NAMESPACE