//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "MotionModel.h"
#include "Utility/MotionTimelineDelegates.h"
#include "Widgets/SMotionTimingPanel.h"
#include "Data/MotionAnimAsset.h"
#include "EditorUndoClient.h"

class UMotionSequenceObject;
class UDebugSkelMeshComponent;
class UAnimSequenceBase;
class FMotionTimelineTrack;
class FMotionTimelineTrack_Tags;
class FMotionTimelineTrack_TagsPanel;
//class FMotionTimelineTrack_Curves;
enum class EFrameNumberDisplayFormats : uint8;

/** Motion Model for an anim sequence base */
class FMotionModel_AnimSequenceBase : public FMotionModel, public FEditorUndoClient
{

private:
	/** The anim sequence base we wrap */
	UAnimSequenceBase* AnimSequenceBase;

	/** Root track for Tags */
	TSharedPtr<FMotionTimelineTrack_Tags> TagRoot;

	/** Legacy tags panel track */
	TSharedPtr<FMotionTimelineTrack_TagsPanel> TagPanel;

	/** Root track for curves */
	//TSharedPtr<FMotionTimelineTrack_Curves> CurveRoot;

	/** Root track for additive layers */
	//TSharedPtr<FMotionTimelineTrack> AdditiveRoot;

	/** Display flags for notifies track */
	bool NotifiesTimingElementNodeDisplayFlags[ETimingElementType::Max];

public:
	FMotionModel_AnimSequenceBase(TObjectPtr<UMotionSequenceObject> InMotionAnim, UDebugSkelMeshComponent* InDebugSkelMesh);
	~FMotionModel_AnimSequenceBase() override;

	/** FAnimModel interface */
	virtual void RefreshTracks() override;
	virtual void Initialize() override;
	virtual void UpdateRange() override;

	/** FEditorUndoClient interface */
	virtual void PostUndo(bool bSuccess) override { HandleUndoRedo(); }
	virtual void PostRedo(bool bSuccess) override { HandleUndoRedo(); }

	const TSharedPtr<FMotionTimelineTrack_Tags>& GetNotifyRoot() const { return TagRoot; }

	/** Delegate used to edit curves */
	FOnEditCurves OnEditCurves;

	/** Notify track timing options */
	bool IsNotifiesTimingElementDisplayEnabled(ETimingElementType::Type ElementType) const;
	void ToggleNotifiesTimingElementDisplayEnabled(ETimingElementType::Type ElementType);

	/** Clamps the sequence to the specified length */
	virtual bool ClampToEndTime(float NewEndTime);

	/** Refresh any simple snap times */
	virtual void RefreshSnapTimes();

protected:
	/** Refresh notify tracks */
	void RefreshNotifyTracks();

	/** Refresh curve tracks */
	//void RefreshCurveTracks();

private:
	/** UI handlers */
	//void EditSelectedCurves();
	//bool CanEditSelectedCurves() const;
	//void RemoveSelectedCurves();
	void SetDisplayFormat(EFrameNumberDisplayFormats InFormat);
	bool IsDisplayFormatChecked(EFrameNumberDisplayFormats InFormat) const;
	void ToggleDisplayPercentage();
	bool IsDisplayPercentageChecked() const;
	void ToggleDisplaySecondary();
	bool IsDisplaySecondaryChecked() const;
	void HandleUndoRedo();
};