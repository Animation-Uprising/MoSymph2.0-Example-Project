//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "MotionTimelineTrack.h"
#include "Widgets/SMotionTimingPanel.h"

class SMotionTagPanel;
class SVerticalBox;
class SInlineEditableTextBlock;

/** A timeline track that re-uses the legacy panel widget to display notifies */
class FMotionTimelineTrack_TagsPanel : public FMotionTimelineTrack
{
	MOTIONTIMELINE_DECLARE_TRACK(FMotionTimelineTrack_TagsPanel, FMotionTimelineTrack);

public:
	static const float NotificationTrackHeight;

	FMotionTimelineTrack_TagsPanel(const TSharedRef<FMotionModel>& InModel);

	/** FMotionTimelineTrack interface */
	virtual TSharedRef<SWidget> GenerateContainerWidgetForTimeline() override;
	virtual TSharedRef<SWidget> GenerateContainerWidgetForOutliner(const TSharedRef<SMotionOutlinerItem>& InRow) override;
	virtual bool SupportsFiltering() const override { return false; }

	TSharedRef<SMotionTagPanel> GetAnimNotifyPanel();

	void Update();

	/** Request a rename next update */
	void RequestTrackRename(int32 InTrackIndex) { PendingRenameTrackIndex = InTrackIndex; }

private:
	TSharedRef<SWidget> BuildNotifiesPanelSubMenu(int32 InTrackIndex);
	void InsertTrack(int32 InTrackIndexToInsert);
	void RemoveTrack(int32 InTrackIndexToRemove);
	void RefreshOutlinerWidget();
	void OnCommitTrackName(const FText& InText, ETextCommit::Type CommitInfo, int32 TrackIndexToName);
	EVisibility OnGetTimingNodeVisibility(ETimingElementType::Type ElementType) const;
	EActiveTimerReturnType HandlePendingRenameTimer(double InCurrentTime, float InDeltaTime, TWeakPtr<SInlineEditableTextBlock> InInlineEditableTextBlock);
	void HandleNotifyChanged();

	/** The legacy notify panel */
	TSharedPtr<SMotionTagPanel> MotionTagPanel;

	/** The outliner widget to allow for dynamic refresh */
	TSharedPtr<SVerticalBox> OutlinerWidget;

	/** Track index we want to trigger a rename for */
	int32 PendingRenameTrackIndex;
};