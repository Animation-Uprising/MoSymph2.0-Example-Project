//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "SMotionTimelineSplitterOverlay.h"
#include "Widgets/Layout/SBox.h"

void SMotionTimelineSplitterOverlay::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::SelfHitTestInvisible);

	Splitter = SArgumentNew(InArgs, SSplitter);
	Splitter->SetVisibility(EVisibility::HitTestInvisible);
	AddSlot()
		[
			Splitter.ToSharedRef()
		];

	for (int32 Index = 0; Index < Splitter->GetChildren()->Num() - 1; ++Index)
	{
		AddSlot()
			.Padding(TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SMotionTimelineSplitterOverlay::GetSplitterHandlePadding, Index)))
			[
				SNew(SBox)
				.Visibility(EVisibility::Visible)
			];
	}
}	

void SMotionTimelineSplitterOverlay::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	FArrangedChildren SplitterChildren(ArrangedChildren.GetFilter());
	Splitter->ArrangeChildren(AllottedGeometry, SplitterChildren);

	SlotPadding.Reset();

	for (int32 Index = 0; Index < SplitterChildren.Num() - 1; ++Index)
	{
		const auto& ThisGeometry = SplitterChildren[Index].Geometry;
		const auto& NextGeometry = SplitterChildren[Index + 1].Geometry;

		if (Splitter->GetOrientation() == EOrientation::Orient_Horizontal)
		{
			SlotPadding.Add(FMargin(
				ThisGeometry.Position.X + ThisGeometry.GetLocalSize().X,
				0,
				AllottedGeometry.Size.X - NextGeometry.Position.X,
				0)
			);
		}
		else
		{
			SlotPadding.Add(FMargin(
				0,
				ThisGeometry.Position.Y + ThisGeometry.GetLocalSize().Y,
				0,
				AllottedGeometry.Size.Y - NextGeometry.Position.Y)
			);
		}
	}

	SOverlay::OnArrangeChildren(AllottedGeometry, ArrangedChildren);
}

FCursorReply SMotionTimelineSplitterOverlay::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	return Splitter->OnCursorQuery(MyGeometry, CursorEvent);
}

FReply SMotionTimelineSplitterOverlay::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply Reply = Splitter->OnMouseButtonDown(MyGeometry, MouseEvent);
	if (Reply.GetMouseCaptor().IsValid())
	{
		// Set us to be the mouse captor so we can forward events properly
		Reply.CaptureMouse(SharedThis(this));
		SetVisibility(EVisibility::Visible);
	}

	return Reply;
}

void SMotionTimelineSplitterOverlay::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	SetVisibility(EVisibility::SelfHitTestInvisible);
	SOverlay::OnMouseCaptureLost(CaptureLostEvent);
}



FReply SMotionTimelineSplitterOverlay::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply Reply = Splitter->OnMouseButtonUp(MyGeometry, MouseEvent);
	if (Reply.ShouldReleaseMouse())
	{
		SetVisibility(EVisibility::SelfHitTestInvisible);
	}
	return Reply;
}

FReply SMotionTimelineSplitterOverlay::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return Splitter->OnMouseMove(MyGeometry, MouseEvent);
}

void SMotionTimelineSplitterOverlay::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	return Splitter->OnMouseLeave(MouseEvent);
}

FMargin SMotionTimelineSplitterOverlay::GetSplitterHandlePadding(int32 Index) const
{
	if (SlotPadding.IsValidIndex(Index))
	{
		return SlotPadding[Index];
	}

	return 0.f;
}

