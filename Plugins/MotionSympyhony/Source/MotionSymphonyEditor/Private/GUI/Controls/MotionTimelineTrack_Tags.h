// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.
#pragma once

#include "MotionTimelineTrack.h"
#include "Widgets/SMotionTimingPanel.h"

class FMotionTimelineTrack_TagsPanel;

class FMotionTimelineTrack_Tags : public FMotionTimelineTrack
{
	MOTIONTIMELINE_DECLARE_TRACK(FMotionTimelineTrack_Tags, FMotionTimelineTrack);

private:
	/** The legacy notifies panel we are linked to */
	TWeakPtr<FMotionTimelineTrack_TagsPanel> TagsPanel;

public:
	FMotionTimelineTrack_Tags(const TSharedRef<FMotionModel>& InModel);

	/** FMotionTimelineTrack interface */
	virtual TSharedRef<SWidget> GenerateContainerWidgetForOutliner(const TSharedRef<SMotionOutlinerItem>& InRow) override;

	/** Get a new, unused track name using the specified anim sequence */
	static FName GetNewTrackName(UAnimSequenceBase* InAnimSequenceBase);
	static FName GetNewTrackName(TObjectPtr<UMotionAnimObject> InAnimAsset);

	void SetMotionTagPanel(const TSharedRef<FMotionTimelineTrack_TagsPanel>& InTagsPanel) { TagsPanel = InTagsPanel; }

	/** Controls timing visibility for notify tracks */
	EVisibility OnGetTimingNodeVisibility(ETimingElementType::Type InType) const;

private:
	/** Populate the track menu */
	TSharedRef<SWidget> BuildNotifiesSubMenu();

	/** Add a new track */
	void AddTrack();

	
};