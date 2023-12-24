//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

//#pragma once
//
//#include "CoreMinimal.h"
//#include "UObject/UObjectGlobals.h"
//#include "Misc/Attribute.h"
//#include "Widgets/DeclarativeSyntaxSupport.h"
//#include "Input/Reply.h"
//#include "Widgets/SWidget.h"
//#include "EditorStyleSet.h"
//#include "Framework/MarqueeRect.h"
//#include "SMotionTrackPanel.h"
////#include "SAnimEditorBase.h"
//#include "AssetData.h"
//#include "Framework/Commands/Commands.h"
//#include "SMotionTimingPanel.h"
//#include "EditorUndoClient.h"
//#include "Controls/MotionModel.h"
//#include "Containers/ArrayView.h"
//#include "SMotionTimingPanel.h"
//
//class FSlateWindowElementList;
//class SMotionTagNode;
//class SMotionTagTrack;
//class SBorder;
//class SScrollBar;
//struct Rect;
//
//DECLARE_DELEGATE_OneParam(FOnSelectionChanged, const TArray<UObject*>&)
//DECLARE_DELEGATE(FOnTrackSelectionChanged)
//DECLARE_DELEGATE(FOnUpdatePanel)
//DECLARE_DELEGATE_RetVal(float, FOnGetScrubValue)
//DECLARE_DELEGATE(FRefreshOffsetsRequest)
//DECLARE_DELEGATE(FDeleteTag)
//DECLARE_DELEGATE_RetVal(bool, FOnGetIsMotionTagSelectionValidForReplacement)
//DECLARE_DELEGATE_TwoParams(FReplaceWithTag, FString, UClass*)
//DECLARE_DELEGATE_TwoParams(FReplaceWithBlueprintTag, FString, FString)
//DECLARE_DELEGATE(FDeselectAllTags)
//DECLARE_DELEGATE_OneParam(FOnGetBlueprintTagData, TArray<FAssetData>&)
//DECLARE_DELEGATE_OneParam(FOnGetNativeTagClasses, TArray<UClass*>&)
//DECLARE_DELEGATE_RetVal_ThreeParams(bool, FOnSnapPosition, float& /*InOutTimeToSnap*/, float /*InSnapMargin*/, TArrayView<const FName> /*InSkippedSnapTypes*/)
//
//class STagEdTrack;
//class FTagDragDropOp;
//
//namespace ETagPasteMode
//{
//	enum Type
//	{
//		MousePosition,
//		OriginalTime
//	};
//}
//
//namespace ETagPasteMultipleMode
//{
//	enum Type
//	{
//		Relative,
//		Absolute
//	};
//}
//
//namespace ETagStateHandleHit
//{
//	enum Type
//	{
//		Start,
//		End,
//		None
//	};
//}
//
//struct FTagMarqueeOperation
//{
//	FTagMarqueeOperation()
//		: Operation(Add)
//		, bActive(false)
//	{
//	}
//
//	enum Type
//	{
//		/** Holding down Ctrl removes nodes */
//		Remove,
//		/** Holding down Shift adds to the selection */
//		Add,
//		/** When nothing is pressed, marquee replaces selection */
//		Replace
//	} Operation;
//
//	bool IsValid() const
//	{
//		return Rect.IsValid() && bActive;
//	}
//
//	void Start(const FVector2D& InStartLocation, FTagMarqueeOperation::Type InOperationType, TArray<TSharedPtr<SMotionTagNode>>& InOriginalSelection)
//	{
//		Rect = FMarqueeRect(InStartLocation);
//		Operation = InOperationType;
//		OriginalSelection = InOriginalSelection;
//	}
//
//	void End()
//	{
//		Rect = FMarqueeRect();
//	}
//
//
//	/** Given a mouse event, figure out what the marquee selection should do based on the state of Shift and Ctrl keys */
//	static FTagMarqueeOperation::Type OperationTypeFromMouseEvent(const FPointerEvent& MouseEvent)
//	{
//		if (MouseEvent.IsControlDown())
//		{
//			return FTagMarqueeOperation::Remove;
//		}
//		else if (MouseEvent.IsShiftDown())
//		{
//			return FTagMarqueeOperation::Add;
//		}
//		else
//		{
//			return FTagMarqueeOperation::Replace;
//		}
//	}
//
//public:
//	/** The marquee rectangle being dragged by the user */
//	FMarqueeRect Rect;
//
//	/** Whether the marquee has been activated, usually by a drag */
//	bool bActive;
//
//	/** The original selection state before the marquee selection */
//	TArray<TSharedPtr<SMotionTagNode>> OriginalSelection;
//};
//
////////////////////////////////////////////////////////////////////////////
//// SMotionTagPanel
//
//class FMotionTagPanelCommands : public TCommands<FMotionTagPanelCommands>
//{
//public:
//	FMotionTagPanelCommands()
//		: TCommands<FMotionTagPanelCommands>("MotionTagPanel", NSLOCTEXT("Contexts", "MotionTagPanel", "Motion Tag Panel"), NAME_None, FEditorStyle::GetStyleSetName())
//	{
//
//	}
//
//	TSharedPtr<FUICommandInfo> DeleteTag;
//
//	TSharedPtr<FUICommandInfo> CopyTags;
//
//	TSharedPtr<FUICommandInfo> PasteTags;
//
//	virtual void RegisterCommands() override;
//};
//
//// @todo anim : register when it's opened for the animsequence
//// broadcast when animsequence changed, so that we refresh for multiple window
//class SMotionTagPanel : public SMotionTrackPanel, public FEditorUndoClient
//{
//public:
//	SLATE_BEGIN_ARGS(SMotionTagPanel)
//		: _Sequence()
//		, _CurrentPosition()
//		, _ViewInputMin()
//		, _ViewInputMax()
//		, _InputMin()
//		, _InputMax()
//		, _OnSetInputViewRange()
//		, _OnSelectionChanged()
//		, _OnGetScrubValue()
//		, _OnRequestRefreshOffsets()
//	{}
//
//	SLATE_ARGUMENT(class UAnimSequenceBase*, Sequence)
//		SLATE_ARGUMENT(float, WidgetWidth)
//		SLATE_ATTRIBUTE(float, CurrentPosition)
//		SLATE_ATTRIBUTE(float, ViewInputMin)
//		SLATE_ATTRIBUTE(float, ViewInputMax)
//		SLATE_ATTRIBUTE(float, InputMin)
//		SLATE_ATTRIBUTE(float, InputMax)
//		SLATE_EVENT(FOnSetInputViewRange, OnSetInputViewRange)
//		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
//		SLATE_EVENT(FOnGetScrubValue, OnGetScrubValue)
//		SLATE_EVENT(FRefreshOffsetsRequest, OnRequestRefreshOffsets)
//		SLATE_EVENT(FOnGetTimingNodeVisibility, OnGetTimingNodeVisibility)
//		SLATE_EVENT(FOnInvokeTab, OnInvokeTab)
//		SLATE_EVENT(FSimpleDelegate, OnTagsChanged)
//		SLATE_EVENT(FOnSnapPosition, OnSnapPosition)
//
//		SLATE_END_ARGS()
//
//		void Construct(const FArguments& InArgs, const TSharedRef<FMotionModel>& InModel);
//	virtual ~SMotionTagPanel();
//
//	void SetSequence(class UAnimSequenceBase* InSequence);
//
//	// Generate a new track name (smallest integer number that isn't currently used)
//	FName GetNewTrackName() const;
//
//	FReply AddTrack();
//	FReply InsertTrack(int32 TrackIndexToInsert);
//	FReply DeleteTrack(int32 TrackIndexToDelete);
//	bool CanDeleteTrack(int32 TrackIndexToDelete);
//
//	// Handler function for renaming a notify track
//	void OnCommitTrackName(const FText& InText, ETextCommit::Type CommitInfo, int32 TrackIndexToName);
//
//	void Update();
//
//	/** Returns the position of the notify node currently being dragged. Returns -1 if no node is being dragged */
//	float CalculateDraggedNodePos() const;
//
//	/**Handler for when a notify node drag has been initiated */
//	FReply OnTagNodeDragStarted(TArray<TSharedPtr<SMotionTagNode>> TagNodes, TSharedRef<SWidget> Decorator, const FVector2D& ScreenCursorPos, const FVector2D& ScreenNodePosition, const bool bDragOnMarker);
//
//	virtual float GetSequenceLength() const override { return Sequence->SequenceLength; }
//
//	void CopySelectedNodesToClipboard() const;
//	void OnPasteNodes(SMotionTagTrack* RequestTrack, float ClickTime, ETagPasteMode::Type PasteMode, ETagPasteMultipleMode::Type MultiplePasteType);
//
//	/** Handler for properties changing on objects */
//	FCoreUObjectDelegates::FOnObjectPropertyChanged::FDelegate OnPropertyChangedHandle;
//	void OnPropertyChanged(UObject* ChangedObject, FPropertyChangedEvent& PropertyEvent);
//
//	/** SWidget Interface */
//	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
//	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
//	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
//	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
//	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
//	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
//	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
//	/** End SWidget Interface */
//
//	void RefreshMarqueeSelectedNodes(const FGeometry& PanelGeo);
//
//	void OnTagObjectChanged(UObject* EditorBaseObj, bool bRebuild);
//
//	/** Check to make sure the current MotionTag selection is a valid selection for replacing (i.e., AnimTags and MotionTagStates aren't mixed together in the selection) */
//	bool IsTagSelectionValidForReplacement();
//
//	/** Handler for replacing with notify */
//	void OnReplaceSelectedWithTag(FString NewTagName, UClass* NewTagClass);
//
//	/** Handler for replacing with notify blueprint */
//	void OnReplaceSelectedWithTagBlueprint(FString NewBlueprintTagName, FString NewBlueprintTagClass);
//
//	void HandleObjectsSelected(const TArray<UObject*>& InObjects);
//
//	TSharedRef<FUICommandList> GetCommandList() const { return WeakCommandList.Pin().ToSharedRef(); }
//
//private:
//	friend struct FScopedSavedTagSelection;
//
//	TWeakPtr<FMotionModel> WeakModel;
//	TSharedPtr<SBorder> PanelArea;
//	TSharedPtr<SScrollBar> TagTrackScrollBar;
//	class UAnimSequenceBase* Sequence;
//	float WidgetWidth;
//	TAttribute<float> CurrentPosition;
//	FOnSelectionChanged OnSelectionChanged;
//	FOnGetScrubValue OnGetScrubValue;
//	FOnGetTimingNodeVisibility OnGetTimingNodeVisibility;
//
//	/** Manager for mouse controlled marquee selection */
//	FTagMarqueeOperation Marquee;
//
//	/** Delegate to request a refresh of the offsets calculated for notifies */
//	FRefreshOffsetsRequest OnRequestRefreshOffsets;
//
//	/** Store the position of a currently dragged node for display across tracks */
//	float CurrentDragXPosition;
//
//	/** Cached list of anim tracks for notify node drag drop */
//	TArray<TSharedPtr<SMotionTagTrack>> TagMotionTracks;
//
//	/** Cached list of Tag editor tracks */
//	TArray<TSharedPtr<STagEdTrack>> TagEditorTracks;
//
//	// this just refresh notify tracks - UI purpose only
//	// do not call this from here. This gets called by asset. 
//	void RefreshTagTracks();
//
//	/** FEditorUndoClient interface */
//	virtual void PostUndo(bool bSuccess) override;
//	virtual void PostRedo(bool bSuccess) override;
//
//	/** Handler for delete command */
//	void OnDeletePressed();
//
//	/** Deletes all currently selected notifies in the panel */
//	void DeleteSelectedNodeObjects();
//
//	/** We support keyboard focus to detect when we should process key commands like delete */
//	virtual bool SupportsKeyboardFocus() const override
//	{
//		return true;
//	}
//
//	// Called when a track changes it's selection; iterates all tracks collecting selected items
//	void OnTrackSelectionChanged();
//
//	// Called to deselect all notifies across all tracks
//	void DeselectAllTags();
//
//	// Binds the UI commands for this widget to delegates
//	void BindCommands();
//
//	/** Populates the given class array with all classes deriving from those originally present
//	 * @param InAssetsToSearch Assets to search to detect child classes
//	 * @param InOutAllowedClassNames Classes to allow, this will be expanded to cover all derived classes of those originally present
//	 */
//	void PopulateTagBlueprintClasses(TArray<FString>& InOutAllowedClasses);
//
//	/** Find blueprints matching allowed classes and all derived blueprints
//	 * @param OutTagData Asset data matching allowed classes and their children
//	 * @param InOutAllowedClassNames Classes to allow, this will be expanded to cover all derived classes of those originally present
//	 */
//	void OnGetTagBlueprintData(TArray<FAssetData>& OutTagData, TArray<FString>* InOutAllowedClassNames);
//
//	/** Find classes that inherit from TagOutermost and add correctly formatted class name to OutAllowedBlueprintClassNames
//	 *  to allow us to find blueprints inherited from those types without loading the blueprints.
//	 *	@param OutClasses Array of classes that inherit from TagOutermost
//	 *	@param TagOutermost Outermost notify class to detect children of
//	 *	@param OutAllowedBlueprintClassNames list of class names to add the native class names to
//	 */
//	void OnGetNativeTagData(TArray<UClass*>& OutClasses, UClass* TagOutermost, TArray<FString>* OutAllowedBlueprintClassNames);
//
//	void OnTagTrackScrolled(float InScrollOffsetFraction);
//
//	virtual void InputViewRangeChanged(float ViewMin, float ViewMax) override;
//
//	/** Delegate used to snap when dragging */
//	FOnSnapPosition OnSnapPosition;
//
//	/** UI commands for this widget */
//	TWeakPtr<FUICommandList> WeakCommandList;
//
//	/** Classes that are known to be derived from blueprint notifies */
//	TArray<FString> TagClassNames;
//
//	/** Classes that are known to be derived from blueprint state notifies */
//	TArray<FString> TagStateClassNames;
//
//	/** Handle to the registered OnPropertyChangedHandle delegate */
//	FDelegateHandle OnPropertyChangedHandleDelegateHandle;
//
//	/** Delegate used to invoke a tab */
//	FOnInvokeTab OnInvokeTab;
//
//	/** Delegate used to inform others that notifies have changed (for timing) */
//	FSimpleDelegate OnTagsChanged;
//
//	/** Recursion guard for selection */
//	bool bIsSelecting;
//
//	/** Recursion guard for updating */
//	bool bIsUpdating;
//};