//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#if ENGINE_MINOR_VERSION > 2
#include "AnimatedRange.h"
#endif
#include "Templates/SharedPointer.h"
#include "ITimeSlider.h"
#include "PersonaDelegates.h"
#include "UObject/GCObject.h"
#include "Containers/ArrayView.h"
#include "Preferences/PersonaOptions.h"
#include "Animation/DebugSkelMeshComponent.h"



class FMotionTimelineTrack;
enum class EViewRangeInterpolation;
class UAnimSequenceBase;
class FPreviewScene;
class FUICommandList;
class UEditorAnimBaseObj;
class UMotionAnimObject;

class FMotionModel : public TSharedFromThis<FMotionModel>, public FGCObject 
{
public:
	TObjectPtr<UMotionAnimObject> MotionAnim;

	UDebugSkelMeshComponent* DebugMesh;

	TWeakPtr<FUICommandList> WeakCommandList;

protected:
	TArray<TSharedRef<FMotionTimelineTrack>> RootTracks;

	TSet<TSharedRef<FMotionTimelineTrack>> SelectedTracks;

	/** Times that can be edited by dragging bars in the UI */
	TArray<double> EditableTimes;

	/** Struct describing the type of a snap */
	struct FSnapType
	{
		FSnapType(const FName& InType, const FText& InDisplayName, TFunction<double(const FMotionModel&, double)> InSnapFunction = nullptr)
			: Type(InType)
			, DisplayName(InDisplayName)
			, SnapFunction(InSnapFunction)
		{}

		/** Identifier for this snap type */
		FName Type;

		/** Display name for this snap type */
		FText DisplayName;

		/** Optional function to snap with */
		TFunction<double(const FMotionModel&, double)> SnapFunction;

		/** Built-in snap types */
		static const FSnapType Frames;
		static const FSnapType Notifies;
		static const FSnapType CompositeSegment;
		static const FSnapType MontageSection;
	};

	/** Struct describing a time that can be snapped to */
	struct FSnapTime
	{
		FSnapTime(const FName& InType, double InTime)
			: Type(InType)
			, Time(InTime)
		{}

		/** Type corresponding to a FSnapType */
		FName Type;

		/** The time to snap to */
		double Time;
	};

	/** Snap types for this model */
	TMap<FName, FSnapType> SnapTypes;

	/** Times that can be snapped to when dragging */
	TArray<FSnapTime> SnapTimes;

	/** Delegate used to edit object details */
	FOnObjectsSelected OnSelectObjects;

	FAnimatedRange ViewRange;
	FAnimatedRange WorkingRange;
	FAnimatedRange PlaybackRange;

	/** Recursion guard for selection */
	bool bIsSelecting;

public:
	FMotionModel(TObjectPtr<UMotionAnimObject> InMotionAnim, UDebugSkelMeshComponent* InDebugSkelMesh);
	~FMotionModel() override {}

#if ENGINE_MINOR_VERSION > 2
	FString GetReferencerName() const override;
#endif

	/** Binds Commands and perform any one-time initialization */
	virtual void Initialize();

	/** Get root tracks representing the tree */
	TArray<TSharedRef<FMotionTimelineTrack>>& GetRootTracks() { return RootTracks; }
	const TArray<TSharedRef<FMotionTimelineTrack>>& GetRootTracks() const { return RootTracks; }

	//View Range
	FAnimatedRange GetViewRange() const;
	void SetViewRange(TRange<double> InRange);
	void SetPlaybackRange(TRange<double> InRange);
	FAnimatedRange GetWorkingRange() const;
	TRange<FFrameNumber> GetPlaybackRange() const;

	void HandleViewRangeChanged(TRange<double> InRange, EViewRangeInterpolation InInterpolation);
	void HandleWorkingRangeChanged(TRange<double> InRange);

	//Time & Scrub
	FFrameNumber GetScrubPosition() const;
	float GetScrubTime() const;
	void SetScrubPosition(FFrameTime NewScrubPosition) const;

	virtual float GetPlayLength() const;

	virtual double GetFrameRate() const;
	virtual int32 GetTickResolution() const;

	virtual UAnimationAsset* GetAnimAsset() const;
	
	DECLARE_EVENT(FMotionModel, FOnTracksChanged)
	FOnTracksChanged& OnTracksChanged() { return OnTracksChangedDelegate; }

	DECLARE_EVENT_OneParam(FMotionModel, FOnHandleObjectsSelected, const TArray<UObject*>& /* InObjects*/)
	FOnHandleObjectsSelected& OnHandleObjectsSelected() { return OnHandleObjectsSelectedDelegate; }

	virtual void RefreshTracks();

	virtual void UpdateRange();

	template<typename AssetType>
	AssetType* GetAsset() const
	{
		return Cast<AssetType>(GetAnimAsset());
	}

	TSharedRef<UDebugSkelMeshComponent> GetDebugSkelMeshComponent() const { return MakeShareable(DebugMesh); }

	TSharedRef<FUICommandList> GetCommandList() const;

	/** Track selection */
	bool IsTrackSelected(const TSharedRef<FMotionTimelineTrack>& InTrack) const;
	void ClearTrackSelection();
	void SetTrackSelected(const TSharedRef<FMotionTimelineTrack>& InTrack, bool bIsSelected);

	UObject* ShowInDetailsView(UClass* EdClass);

	void SelectObjects(const TArray<UObject*>& Objects);

	void ClearDetailsView();

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual void RecalculateSequenceLength();

	float CalculateSequenceLengthOfEditorObjects() const;

	virtual void InitDetailsViewEditorObjects(UEditorAnimBaseObj* EdObj);

	/** Editable Times */
	const TArray<double>& GetEditableTimes() const { return EditableTimes; }
	void SetEditableTimes(const TArray<double>& InTimes) { EditableTimes = InTimes; }

	void SetEditableTime(int32 TimeIndex, double Time, bool bIsDragging); 

	virtual void OnSetEditableTime(int32 TimeIndex, double Time, bool bIsDragging);

	//Snap
	bool Snap(float& InOutTime, float InSnapMargin, TArrayView<const FName> InSkippedSnapTypes) const;
	bool Snap(double& InOutTime, double InSnapMargin, TArrayView<const FName> InSkippedSnapType) const;

	void ToggleSnap(FName InSnapName);
	bool IsSnapChecked(FName InSnapName) const;
	bool IsSnapAvailable(FName InSnapName) const;

	virtual void BuildContextMenu(FMenuBuilder& InMenuBuilder);

protected:
	/** Delegate fired when tracks change */
	FOnTracksChanged OnTracksChangedDelegate;

	/** Delegate fired when selection changes */
	FOnHandleObjectsSelected OnHandleObjectsSelectedDelegate;
};