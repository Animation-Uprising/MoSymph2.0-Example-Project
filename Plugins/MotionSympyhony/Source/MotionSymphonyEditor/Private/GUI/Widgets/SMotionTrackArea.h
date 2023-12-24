//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SlotBase.h"
#include "Layout/Children.h"
#include "Widgets/SPanel.h"

class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;
class FMotionTimelineTrack;
class SMotionTrack;
class SMotionOutliner;
class FMotionModel;
class FMotionTimeSliderController;

/**
 * Structure representing a slot in the track area.
 */
class FMotionTrackAreaSlot : public TSlotBase<FMotionTrackAreaSlot>
{
public:


	/** Horizontal/Vertical alignment for the slot. */
	EHorizontalAlignment HAlignment;
	EVerticalAlignment VAlignment;

	/** The track that we represent. */
	TWeakPtr<SMotionTrack> TrackWidget;

public:
	/** Construction from a track lane */
	FMotionTrackAreaSlot(const TSharedPtr<SMotionTrack>& InSlotContent);

	/** Get the vertical position of this slot inside its parent. */
	float GetVerticalOffset() const;

	EHorizontalAlignment GetHorizontalAlignment() const;
	EVerticalAlignment GetVerticalAlignment() const;
};

class SMotionTrackArea : public SPanel
{
public:
	
	SLATE_BEGIN_ARGS(SMotionTrackArea)
	{
		_Clipping = EWidgetClipping::ClipToBounds;
	}
	SLATE_END_ARGS()

private:
	/** Cached geometry. */
	FGeometry CachedGeometry;

	/** A map of child slot content that exists in our view. */
	TMap<TWeakPtr<FMotionTimelineTrack>, TWeakPtr<SMotionTrack>> TrackSlots;

	/** Weak pointer to the model. */
	TWeakPtr<FMotionModel> WeakModel;

	/** Weak pointer to the time slider. */
	TWeakPtr<FMotionTimeSliderController> WeakTimeSliderController;

	/** Weak pointer to the outliner (used for scrolling interactions). */
	TWeakPtr<SMotionOutliner> WeakOutliner;

	/** The track area's children. */
	TPanelChildren<FMotionTrackAreaSlot> Children;

public:
	SMotionTrackArea()
		: Children(this)
	{
	}

	void Construct(const FArguments& InArgs, const TSharedRef<FMotionModel>& InMotionModel, 
		const TSharedRef<FMotionTimeSliderController>& InTimeSliderController);

	void Empty();
	void AddTrackSlot(const TSharedRef<FMotionTimelineTrack>& InTrack, 
		const TSharedPtr<SMotionTrack>& InTrackWidget);

	TSharedPtr<SMotionTrack> FindTrackSlot(const TSharedRef<FMotionTimelineTrack>& InTrack);

	const FGeometry& GetCachedGeometry() const { return CachedGeometry; }

	void SetOutliner(const TSharedPtr<SMotionOutliner> InOutliner);

	/** SWidget interface */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FChildren* GetChildren() override;

protected:
	void UpdateHoverStates(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
};