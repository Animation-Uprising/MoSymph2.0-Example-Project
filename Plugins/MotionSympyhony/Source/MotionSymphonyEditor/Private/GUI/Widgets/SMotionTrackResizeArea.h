//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FMotionTimelineTrack;

class SMotionTrackResizeArea : public SCompoundWidget
{

public:
	SLATE_BEGIN_ARGS(SMotionTrackResizeArea) {}
	SLATE_END_ARGS()
	
private:
	struct FDragParameters
	{
		FDragParameters(float InOriginalHeight, float InDragStartY)
			: OriginalHeight(InOriginalHeight)
			, DragStartY(InDragStartY)
		{}

		float OriginalHeight;
		float DragStartY;
	};

	TOptional<FDragParameters> DragParameters;
	TWeakPtr<FMotionTimelineTrack> Track;

public:
	void Construct(const FArguments& InArgs, TWeakPtr<FMotionTimelineTrack> InTrack);

	/** SWidget interface */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
};