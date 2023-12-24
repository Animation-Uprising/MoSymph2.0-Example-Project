//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.


#include "SMotionTagPanel.h"
#include "Rendering/DrawElements.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/PropertyPortFlags.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Animation/AnimSequence.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Input/SButton.h"
#include "Animation/AnimMontage.h"
#include "Animation/EditorNotifyObject.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "ScopedTransaction.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetSelection.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "BlueprintActionDatabase.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/BlendSpace.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Modules/ModuleManager.h"
#include "IEditableSkeleton.h"
#include "ISkeletonEditorModule.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "IAnimationEditor.h"
#include "IAnimationSequenceBrowser.h"
#include "Controls/MotionTimelineTrack_TagsPanel.h"
#include "Data/MotionAnimAsset.h"
#include "Objects/MotionAnimObject.h"
#include "Objects/Tags/TagPoint.h"
#include "Objects/Tags/TagSection.h"


// AnimNotify Drawing
const float NotifyHeightOffset = 0.f;
const float NotifyHeight = FMotionTimelineTrack_TagsPanel::NotificationTrackHeight;
const FVector2D ScrubHandleSize(12.0f, 12.0f);
const FVector2D AlignmentMarkerSize(10.f, 20.f);
const FVector2D TextBorderSize(1.f, 1.f);

#define LOCTEXT_NAMESPACE "MotionTagPanel"

DECLARE_DELEGATE_OneParam(FOnDeleteTag, struct FAnimNotifyEvent*)
DECLARE_DELEGATE_RetVal_FourParams(FReply, FOnNotifyNodeDragStarted, TSharedRef<SMotionTagNode>, const FPointerEvent&, const FVector2D&, const bool)
DECLARE_DELEGATE_RetVal_FiveParams(FReply, FOnNotifyNodesDragStarted, TArray<TSharedPtr<SMotionTagNode>>, TSharedRef<SWidget>, const FVector2D&, const FVector2D&, const bool)
DECLARE_DELEGATE_RetVal(float, FOnGetDraggedNodePos)
DECLARE_DELEGATE_TwoParams(FPanTrackRequest, int32, FVector2D)
DECLARE_DELEGATE(FCopyNodes)
DECLARE_DELEGATE_FourParams(FPasteNodes, SMotionTagTrack*, float, ETagPasteMode::Type, ETagPasteMultipleMode::Type)
DECLARE_DELEGATE_RetVal_OneParam(EVisibility, FOnGetTimingNodeVisibilityForNode, TSharedPtr<SMotionTagNode>)

class FTagDragDropOp;

FText MakeTooltipFromTime(const TObjectPtr<UMotionAnimObject> InMotionAnim, float InSeconds, float InDuration)
{
	const UAnimSequenceBase* MotionSequence = Cast<UAnimSequenceBase>(InMotionAnim->AnimAsset);



	const FText Frame = MotionSequence ? FText::AsNumber(MotionSequence->GetFrameAtTime(InSeconds)) : FText::AsNumber(0);
	const FText Seconds = FText::AsNumber(InSeconds);

	if (InDuration > 0.0f)
	{
		const FText Duration = FText::AsNumber(InDuration);
		return FText::Format(LOCTEXT("NodeToolTipLong", "@ {0} sec (frame {1}) for {2} sec"), Seconds, Frame, Duration);
	}
	else
	{
		return FText::Format(LOCTEXT("NodeToolTipShort", "@ {0} sec (frame {1})"), Seconds, Frame);
	}
}

// Read common info from the clipboard
bool ReadNotifyPasteHeader(FString& OutPropertyString, const TCHAR*& OutBuffer, float& OutOriginalTime, float& OutOriginalLength, int32& OutTrackSpan)
{
	OutBuffer = nullptr;
	OutOriginalTime = -1.0f;
	
	FPlatformApplicationMisc::ClipboardPaste(OutPropertyString);

	if (!OutPropertyString.IsEmpty())
	{
		//Remove header text
		const FString HeaderString(TEXT("COPY_ANIMNOTIFYEVENT"));

		//Check for string identifier in order to determine whether the text represents an FAnimNotifyEvent.
		if (OutPropertyString.StartsWith(HeaderString) && OutPropertyString.Len() > HeaderString.Len())
		{
			const int32 HeaderSize = HeaderString.Len();
			OutBuffer = *OutPropertyString;
			OutBuffer += HeaderSize;

			FString ReadLine;
			// Read the original time from the first notify
			FParse::Line(&OutBuffer, ReadLine);
			FParse::Value(*ReadLine, TEXT("OriginalTime="), OutOriginalTime);
			FParse::Value(*ReadLine, TEXT("OriginalLength="), OutOriginalLength);
			FParse::Value(*ReadLine, TEXT("TrackSpan="), OutTrackSpan);
			return true;
		}
	}

	return false;
}

namespace ENodeObjectTypes
{
	enum Type
	{
		NOTIFY,
		SYNC_MARKER
	};
};

struct INodeObjectInterface
{
	virtual ENodeObjectTypes::Type GetType() const = 0;
	virtual FAnimNotifyEvent* GetNotifyEvent() = 0;
	virtual int GetTrackIndex() const = 0;
	virtual float GetTime(EAnimLinkMethod::Type ReferenceFrame = EAnimLinkMethod::Absolute) const = 0;
	virtual float GetDuration() = 0;
	virtual FName GetName() = 0;
	virtual TOptional<FLinearColor> GetEditorColor() = 0;
	virtual FText GetNodeTooltip(const TObjectPtr<UMotionAnimObject> MotionAnim) = 0;
	virtual TOptional<UObject*> GetObjectBeingDisplayed() = 0;
	virtual bool IsBranchingPoint() = 0;
	bool operator<(const INodeObjectInterface& Rhs) const { return GetTime() < Rhs.GetTime(); }

	virtual void SetTime(float Time, EAnimLinkMethod::Type ReferenceFrame = EAnimLinkMethod::Absolute) = 0;
	virtual void SetDuration(float Duration) = 0;

	virtual void HandleDrop(TObjectPtr<UMotionAnimObject> MotionAnim, float Time, int32 TrackIndex) = 0;
	virtual void CacheName() = 0;

	virtual void Delete(TObjectPtr<UMotionAnimObject> MotionAnim) = 0;
	virtual void MarkForDelete(TObjectPtr<UMotionAnimObject> MotionAnim) = 0;

	virtual void ExportForCopy(TObjectPtr<UMotionAnimObject> MotionAnim, FString& StrValue) const = 0;

	virtual FGuid GetGuid() const = 0;
};

struct FTagNodeInterface : public INodeObjectInterface
{
	FAnimNotifyEvent* NotifyEvent;

	// Cached notify name (can be generated by blueprints so want to cache this instead of hitting VM) 
	FName CachedNotifyName;

	// Stable Guid that allows us to refer to notify event
	FGuid Guid;

	FTagNodeInterface(FAnimNotifyEvent* InAnimNotifyEvent) : NotifyEvent(InAnimNotifyEvent), Guid(NotifyEvent->Guid) {}
	virtual ENodeObjectTypes::Type GetType() const override { return ENodeObjectTypes::NOTIFY; }
	virtual FAnimNotifyEvent* GetNotifyEvent() override { return NotifyEvent; }
	virtual int GetTrackIndex() const override { return NotifyEvent->TrackIndex; }
	virtual float GetTime(EAnimLinkMethod::Type ReferenceFrame = EAnimLinkMethod::Absolute) const override { return NotifyEvent->GetTime(ReferenceFrame); }
	virtual float GetDuration() override { return NotifyEvent->GetDuration(); }
	virtual FName GetName() override { return CachedNotifyName; }
	virtual bool IsBranchingPoint() override { return NotifyEvent->IsBranchingPoint(); }
	virtual TOptional<FLinearColor> GetEditorColor() override
	{
		TOptional<FLinearColor> ReturnColour;
		if (NotifyEvent->Notify)
		{
			ReturnColour = NotifyEvent->Notify->GetEditorColor();
		}
		else if (NotifyEvent->NotifyStateClass)
		{
			ReturnColour = NotifyEvent->NotifyStateClass->GetEditorColor();
		}
		return ReturnColour;
	}

	virtual FText GetNodeTooltip(const TObjectPtr<UMotionAnimObject> MotionAnim) override
	{
		FText ToolTipText = MakeTooltipFromTime(MotionAnim, NotifyEvent->GetTime(), NotifyEvent->GetDuration());

		UObject* NotifyToDisplayClassOf = NotifyEvent->Notify;
		if (NotifyToDisplayClassOf == nullptr)
		{
			NotifyToDisplayClassOf = NotifyEvent->NotifyStateClass;
		}

		if (NotifyToDisplayClassOf != nullptr)
		{
			ToolTipText = FText::Format(LOCTEXT("MotionTag_ToolTipTagClass", "{0}\nClass: {1}"), ToolTipText, NotifyToDisplayClassOf->GetClass()->GetDisplayNameText());
		}

		return ToolTipText;
	}

	virtual TOptional<UObject*> GetObjectBeingDisplayed() override
	{
		if (NotifyEvent->Notify)
		{
			return TOptional<UObject*>(NotifyEvent->Notify);
		}

		if (NotifyEvent->NotifyStateClass)
		{
			return TOptional<UObject*>(NotifyEvent->NotifyStateClass);
		}
		return TOptional<UObject*>();
	}

	virtual void SetTime(float Time, EAnimLinkMethod::Type ReferenceFrame = EAnimLinkMethod::Absolute) override { NotifyEvent->SetTime(Time, ReferenceFrame); }
	virtual void SetDuration(float Duration) override { NotifyEvent->SetDuration(Duration); }

	virtual void HandleDrop(TObjectPtr<UMotionAnimObject> MotionAnim, float Time, int32 TrackIndex) override
	{
		float EventDuration = NotifyEvent->GetDuration();

		UAnimSequenceBase* MotionSequence = Cast<UAnimSequenceBase>(MotionAnim->AnimAsset);

		NotifyEvent->Link(MotionSequence, Time, NotifyEvent->GetSlotIndex());
		NotifyEvent->RefreshTriggerOffset(MotionSequence->CalculateOffsetForNotify(NotifyEvent->GetTime()));

		if (EventDuration > 0.0f)
		{
			NotifyEvent->EndLink.Link(MotionSequence, NotifyEvent->GetTime() + EventDuration, NotifyEvent->GetSlotIndex());
			NotifyEvent->RefreshEndTriggerOffset(MotionSequence->CalculateOffsetForNotify(NotifyEvent->EndLink.GetTime()));
		}
		else
		{
			NotifyEvent->EndTriggerTimeOffset = 0.0f;
		}

		NotifyEvent->TrackIndex = TrackIndex;
	}

	virtual void CacheName()
	{
		if (NotifyEvent->Notify)
		{
			CachedNotifyName = FName(*NotifyEvent->Notify->GetNotifyName());
		}
		else if (NotifyEvent->NotifyStateClass)
		{
			CachedNotifyName = FName(*NotifyEvent->NotifyStateClass->GetNotifyName());
		}
		else
		{
			CachedNotifyName = NotifyEvent->NotifyName;
		}
	}

	virtual void Delete(TObjectPtr<UMotionAnimObject> MotionAnim)
	{
		MotionAnim->Modify();
		for (int32 I = 0; I < MotionAnim->Tags.Num(); ++I)
		{
			if (NotifyEvent == &(MotionAnim->Tags[I]))
			{
				MotionAnim->Tags.RemoveAt(I);
				break;
			}
		}
	}

	virtual void MarkForDelete(TObjectPtr<UMotionAnimObject> MotionAnim)
	{
		for (int32 I = 0; I < MotionAnim->Tags.Num(); ++I)
		{
			if (NotifyEvent == &(MotionAnim->Tags[I]))
			{
				MotionAnim->Tags[I].Guid = FGuid();
				break;
			}
		}
	}

	virtual void ExportForCopy(TObjectPtr<UMotionAnimObject> MotionAnim, FString& StrValue) const override
	{
		int32 Index = INDEX_NONE;
		for (int32 TagIndex = 0; TagIndex < MotionAnim->Tags.Num(); ++TagIndex)
		{
			if (NotifyEvent == &MotionAnim->Tags[TagIndex])
			{
				Index = TagIndex;
				break;
			}
		}

		check(Index != INDEX_NONE);

		FArrayProperty* ArrayProperty = nullptr;
		uint8* PropertyData = MotionAnim->FindTagPropertyData(Index, ArrayProperty);
		if (PropertyData && ArrayProperty)
		{
			ArrayProperty->Inner->ExportTextItem_Direct(StrValue, PropertyData, PropertyData, MotionAnim, PPF_Copy);
		}
	}

	virtual FGuid GetGuid() const override
	{
		return Guid;
	}

	static void RemoveInvalidNotifies(TObjectPtr<UMotionAnimObject> MotionAnim)
	{
		MotionAnim->Modify();
		MotionAnim->Tags.RemoveAll([](const FAnimNotifyEvent& InNotifyEvent) { return !InNotifyEvent.Guid.IsValid(); });
	}
};

// Struct that allows us to get the max value of 2 numbers at compile time
template<int32 A, int32 B>
struct CompileTimeMax
{
	enum Max { VALUE = (A > B) ? A : B };
};

// Size of biggest object that we can store in our node, if new node interfaces are added they should be part of this calculation
const int32 MAX_NODE_OBJECT_INTERFACE_SIZE = CompileTimeMax<sizeof(FTagNodeInterface), sizeof(FTagNodeInterface/*FSyncMarkerNodeInterface*/)>::VALUE;


//////////////////////////////////////////////////////////////////////////
// SMotionTagNode

class SMotionTagNode : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMotionTagNode)
		: _MotionAnim()
		, _AnimNotify(nullptr)
		, _OnNodeDragStarted()
		, _OnUpdatePanel()
		, _PanTrackRequest()
		, _OnSelectionChanged()
		, _ViewInputMin()
		, _ViewInputMax()
	{
	}
	SLATE_ARGUMENT(struct TObjectPtr<UMotionAnimObject>, MotionAnim)
		SLATE_ARGUMENT(FAnimNotifyEvent*, AnimNotify)
		SLATE_EVENT(FOnNotifyNodeDragStarted, OnNodeDragStarted)
		SLATE_EVENT(FOnUpdatePanel, OnUpdatePanel)
		SLATE_EVENT(FPanTrackRequest, PanTrackRequest)
		SLATE_EVENT(FOnTrackSelectionChanged, OnSelectionChanged)
		SLATE_ATTRIBUTE(float, ViewInputMin)
		SLATE_ATTRIBUTE(float, ViewInputMax)
		SLATE_ARGUMENT(TSharedPtr<SMotionTimingNode>, StateEndTimingNode)
		SLATE_EVENT(FOnSnapPosition, OnSnapPosition)
		SLATE_END_ARGS()

		void Construct(const FArguments& Declaration);

	// SWidget interface
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	// End of SWidget interface

	void ExportForCopy();

	// SNodePanel::SNode interface
	void UpdateSizeAndPosition(const FGeometry& AllottedGeometry);
	FVector2D GetWidgetPosition() const;
	FVector2D GetNotifyPosition() const;
	FVector2D GetNotifyPositionOffset() const;
	FVector2D GetSize() const;
	bool HitTest(const FGeometry& AllottedGeometry, FVector2D MouseLocalPose) const;

	// Extra hit testing to decide whether or not the duration handles were hit on a state node
	ETagStateHandleHit::Type DurationHandleHitTest(const FVector2D& CursorScreenPosition) const;

	UObject* GetObjectBeingDisplayed() const;
	// End of SNodePanel::SNode

	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const /*override*/;

	/** Helpers to draw scrub handles and snap offsets */
	void DrawHandleOffset(const float& Offset, const float& HandleCentre, FSlateWindowElementList& OutDrawElements, int32 MarkerLayer, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FLinearColor NodeColour) const;
	void DrawScrubHandle(float ScrubHandleCentre, FSlateWindowElementList& OutDrawElements, int32 ScrubHandleID, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FLinearColor NodeColour) const;

	FLinearColor GetNotifyColor() const;
	FText GetNotifyText() const;

	/** Node object interface */
	INodeObjectInterface* NodeObjectInterface;

	/** In object storage for our interface struct, saves us having to dynamically allocate what will be a very small struct*/
	uint8 NodeObjectInterfaceStorage[MAX_NODE_OBJECT_INTERFACE_SIZE];

	/** Helper function to create our node interface object */
	template<typename InterfaceType, typename ParamType>
	void MakeNodeInterface(ParamType& InParam)
	{
		check(sizeof(InterfaceType) <= MAX_NODE_OBJECT_INTERFACE_SIZE); //Not enough space, check definiton of MAX_NODE_OBJECT_INTERFACE_SIZE
		NodeObjectInterface = new(NodeObjectInterfaceStorage)InterfaceType(InParam);
	}

	void DropCancelled();

	/** Returns the size of this notifies duration in screen space */
	float GetDurationSize() const { return NotifyDurationSizeX; }

	/** Sets the position the mouse was at when this node was last hit */
	void SetLastMouseDownPosition(const FVector2D& CursorPosition) { LastMouseDownPosition = CursorPosition; }

	/** The minimum possible duration that a notify state can have */
	static const float MinimumStateDuration;

	const FVector2D& GetScreenPosition() const
	{
		return ScreenPosition;
	}

	const float GetLastSnappedTime() const
	{
		return LastSnappedTime;
	}

	void ClearLastSnappedTime()
	{
		LastSnappedTime = -1.0f;
	}

	void SetLastSnappedTime(float NewSnapTime)
	{
		LastSnappedTime = NewSnapTime;
	}

private:
	FText GetNodeTooltip() const;

	/** Detects any overflow on the anim notify track and requests a track pan */
	float HandleOverflowPan(const FVector2D& ScreenCursorPos, float TrackScreenSpaceXPosition, float TrackScreenSpaceMin, float TrackScreenSpaceMax);

	/** Finds a snap position if possible for the provided scrub handle, if it is not possible, returns -1.0f */
	float GetScrubHandleSnapPosition(float NotifyInputX, ETagStateHandleHit::Type HandleToCheck);

	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override;

	/** The sequence that the AnimNotifyEvent for Notify lives in */
	TObjectPtr<UMotionAnimObject> MotionAnim;
	FSlateFontInfo Font;

	TAttribute<float> ViewInputMin;
	TAttribute<float> ViewInputMax;
	FVector2D CachedAllotedGeometrySize;
	FVector2D ScreenPosition;
	float LastSnappedTime;

	bool bDrawTooltipToRight;
	bool bBeingDragged;
	bool bSelected;

	// Index for undo transactions for dragging, as a check to make sure it's active
	int32 DragMarkerTransactionIdx;

	/** The scrub handle currently being dragged, if any */
	ETagStateHandleHit::Type CurrentDragHandle;

	float NotifyTimePositionX;
	float NotifyDurationSizeX;
	float NotifyScrubHandleCentre;

	float WidgetX;
	FVector2D WidgetSize;

	FVector2D TextSize;
	float LabelWidth;
	FVector2D BranchingPointIconSize;

	/** Last position the user clicked in the widget */
	FVector2D LastMouseDownPosition;

	/** Delegate that is called when the user initiates dragging */
	FOnNotifyNodeDragStarted OnNodeDragStarted;

	/** Delegate to pan the track, needed if the markers are dragged out of the track */
	FPanTrackRequest PanTrackRequest;

	/** Delegate used to snap positions */
	FOnSnapPosition OnSnapPosition;

	/** Delegate to signal selection changing */
	FOnTrackSelectionChanged OnSelectionChanged;

	/** Delegate to redraw the notify panel */
	FOnUpdatePanel OnUpdatePanel;

	/** Cached owning track geometry */
	FGeometry CachedTrackGeometry;

	TSharedPtr<SOverlay> EndMarkerNodeOverlay;

	friend class SMotionTagTrack;
};

class SMotionTagPair : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMotionTagPair)
	{}

	SLATE_NAMED_SLOT(FArguments, LeftContent)

		SLATE_ARGUMENT(TSharedPtr<SMotionTagNode>, Node);
		SLATE_EVENT(FOnGetTimingNodeVisibilityForNode, OnGetTimingNodeVisibilityForNode)

		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	float GetWidgetPaddingLeft();

protected:
	TSharedPtr<SWidget> PairedWidget;
	TSharedPtr<SMotionTagNode> NodePtr;
};

void SMotionTagPair::Construct(const FArguments& InArgs)
{
	NodePtr = InArgs._Node;
	PairedWidget = InArgs._LeftContent.Widget;
	check(NodePtr.IsValid());
	check(PairedWidget.IsValid());

	float ScaleMult = 1.0f;
	FVector2D NodeSize = NodePtr->ComputeDesiredSize(ScaleMult);
	SetVisibility(EVisibility::SelfHitTestInvisible);

	this->ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.HAlign(EHorizontalAlignment::HAlign_Center)
		.VAlign(EVerticalAlignment::VAlign_Center)
		[
			PairedWidget->AsShared()
		]
		]
	+ SHorizontalBox::Slot()
		[
			NodePtr->AsShared()
		]
		];
}

float SMotionTagPair::GetWidgetPaddingLeft()
{
	return NodePtr->GetWidgetPosition().X - PairedWidget->GetDesiredSize().X;
}

//////////////////////////////////////////////////////////////////////////
// SMotionTagTrack

class SMotionTagTrack : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMotionTagTrack)
		: _MotionAnim(NULL)
		, _ViewInputMin()
		, _ViewInputMax()
		, _TrackIndex()
		, _TrackColor(FLinearColor::White)
		, _OnSelectionChanged()
		, _OnUpdatePanel()
		, _OnGetTagBlueprintData()
		, _OnGetTagStateBlueprintData()
		, _OnGetTagNativeClasses()
		, _OnGetTagStateNativeClasses()
		, _OnGetScrubValue()
		, _OnGetDraggedNodePos()
		, _OnNodeDragStarted()
		, _OnRequestTrackPan()
		, _OnRequestOffsetRefresh()
		, _OnDeleteTag()
		, _OnGetIsMotionTagSelectionValidForReplacement()
		, _OnReplaceSelectedWithTag()
		, _OnReplaceSelectedWithBlueprintTag()
		, _OnDeselectAllTags()
		, _OnCopyNodes()
		, _OnPasteNodes()
		, _OnSetInputViewRange()
	{}

	SLATE_ARGUMENT(struct TObjectPtr<UMotionAnimObject>, MotionAnim)
		SLATE_ARGUMENT(TArray<FAnimNotifyEvent*>, AnimNotifies)
		SLATE_ATTRIBUTE(float, ViewInputMin)
		SLATE_ATTRIBUTE(float, ViewInputMax)
		SLATE_EVENT(FOnSnapPosition, OnSnapPosition)
		SLATE_ARGUMENT(int32, TrackIndex)
		SLATE_ARGUMENT(FLinearColor, TrackColor)
		SLATE_ATTRIBUTE(EVisibility, QueuedNotifyTimingNodeVisibility)
		SLATE_ATTRIBUTE(EVisibility, BranchingPointTimingNodeVisibility)
		SLATE_EVENT(FOnTrackSelectionChanged, OnSelectionChanged)
		SLATE_EVENT(FOnUpdatePanel, OnUpdatePanel)
		SLATE_EVENT(FOnGetBlueprintTagData, OnGetTagBlueprintData)
		SLATE_EVENT(FOnGetBlueprintTagData, OnGetTagStateBlueprintData)
		SLATE_EVENT(FOnGetNativeTagClasses, OnGetTagNativeClasses)
		SLATE_EVENT(FOnGetNativeTagClasses, OnGetTagStateNativeClasses)
		SLATE_EVENT(FOnGetScrubValue, OnGetScrubValue)
		SLATE_EVENT(FOnGetDraggedNodePos, OnGetDraggedNodePos)
		SLATE_EVENT(FOnNotifyNodesDragStarted, OnNodeDragStarted)
		SLATE_EVENT(FPanTrackRequest, OnRequestTrackPan)
		SLATE_EVENT(FRefreshOffsetsRequest, OnRequestOffsetRefresh)
		SLATE_EVENT(FDeleteTag, OnDeleteTag)
		SLATE_EVENT(FOnGetIsMotionTagSelectionValidForReplacement, OnGetIsMotionTagSelectionValidForReplacement)
		SLATE_EVENT(FReplaceWithTag, OnReplaceSelectedWithTag)
		SLATE_EVENT(FReplaceWithBlueprintTag, OnReplaceSelectedWithBlueprintTag)
		SLATE_EVENT(FDeselectAllTags, OnDeselectAllTags)
		SLATE_EVENT(FCopyNodes, OnCopyNodes)
		SLATE_EVENT(FPasteNodes, OnPasteNodes)
		SLATE_EVENT(FOnSetInputViewRange, OnSetInputViewRange)
		SLATE_EVENT(FOnGetTimingNodeVisibility, OnGetTimingNodeVisibility)
		SLATE_EVENT(FOnInvokeTab, OnInvokeTab)
		SLATE_ARGUMENT(TSharedPtr<FUICommandList>, CommandList)
		SLATE_END_ARGS()
public:

	/** Type used for list widget of tracks */
	void Construct(const FArguments& InArgs);

	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override { UpdateCachedGeometry(AllottedGeometry); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}
	// End of SWidget interface

	/**
	 * Update the nodes to match the data that the panel is observing
	 */
	void Update();

	/** Returns the cached rendering geometry of this track */
	const FGeometry& GetCachedGeometry() const { return CachedGeometry; }

	FTrackScaleInfo GetCachedScaleInfo() const { return FTrackScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0.f, 0.f, CachedGeometry.GetLocalSize()); }

	/** Updates sequences when a notify node has been successfully dragged to a new position
	 *	@param Offset - Offset from the widget to the time handle
	 */
	void HandleNodeDrop(TSharedPtr<SMotionTagNode> Node, float Offset = 0.0f);

	// Number of nodes in the track currently selected
	int32 GetNumSelectedNodes() const { return SelectedNodeIndices.Num(); }

	// Index of the track in the notify panel
	int32 GetTrackIndex() const { return TrackIndex; }

	// Time at the position of the last mouseclick
	float GetLastClickedTime() const { return LastClickedTime; }

	// Removes the node widgets from the track and adds them to the provided Array
	void DisconnectSelectedNodesForDrag(TArray<TSharedPtr<SMotionTagNode>>& DragNodes);

	// Adds our current selection to the provided set
	void AppendSelectionToSet(FGraphPanelSelectionSet& SelectionSet);
	// Adds our current selection to the provided array
	void AppendSelectionToArray(TArray<INodeObjectInterface*>& Selection) const;
	// Gets the currently selected SMotionTagNode instances
	void AppendSelectedNodeWidgetsToArray(TArray<TSharedPtr<SMotionTagNode>>& NodeArray) const;
	// Gets the indices of the selected notifies
	const TArray<int32>& GetSelectedNotifyIndices() const { return SelectedNodeIndices; }

	INodeObjectInterface* GetNodeObjectInterface(int32 NodeIndex) { return NotifyNodes[NodeIndex]->NodeObjectInterface; }
	/**
	* Deselects all currently selected notify nodes
	* @param bUpdateSelectionSet - Whether we should report a selection change to the panel
	*/
	void DeselectAllNotifyNodes(bool bUpdateSelectionSet = true);

	/** Select all nodes contained in the supplied Guid set. */
	void SelectNodesByGuid(const TSet<FGuid>& InGuids, bool bUpdateSelectionSet);

	/** Get the number of notify nodes we contain */
	int32 GetNumNotifyNodes() const { return NotifyNodes.Num(); }

	/** Check whether a node is selected */
	bool IsNodeSelected(int32 NodeIndex) const { return NotifyNodes[NodeIndex]->bSelected; }

	// get Property Data of one element (NotifyIndex) from Notifies property of Sequence
	static uint8* FindNotifyPropertyData(UAnimSequenceBase* Sequence, int32 NotifyIndex, FArrayProperty*& ArrayProperty);

	// Paste a single Notify into this track from an exported string
	void PasteSingleNotify(FString& NotifyString, float PasteTime);

	// Uses the given track space rect and marquee information to refresh selection information
	void RefreshMarqueeSelectedNodes(FSlateRect& Rect, FTagMarqueeOperation& Marquee);

	// Create new notifies
	FAnimNotifyEvent& CreateNewBlueprintNotify(FString NewNotifyName, FString BlueprintPath, float StartTime);
	FAnimNotifyEvent& CreateNewNotify(FString NewNotifyName, UClass* NotifyClass, float StartTime);

	// Get the Blueprint Class from the path of the Blueprint
	static TSubclassOf<UObject> GetBlueprintClassFromPath(FString BlueprintPath);

	// Get the default Notify Name for a given blueprint notify asset
	FString MakeBlueprintNotifyName(const FString& InNotifyClassName);

	// Need to make sure tool tips are cleared during node clear up so slate system won't
	// call into invalid notify.
	void ClearNodeTooltips();

protected:

	// Build up a "New Notify..." menu
	template<typename NotifyTypeClass>
	void MakeNewNotifyPicker(FMenuBuilder& MenuBuilder, bool bIsReplaceWithMenu = false);
	void FillNewNotifyMenu(FMenuBuilder& MenuBuilderbool, bool bIsReplaceWithMenu = false);
	void FillNewNotifyStateMenu(FMenuBuilder& MenuBuilder, bool bIsReplaceWithMenu = false);

	// New notify functions
	void CreateNewBlueprintNotifyAtCursor(FString NewNotifyName, FString BlueprintPath);
	void CreateNewNotifyAtCursor(FString NewNotifyName, UClass* NotifyClass);
	void OnNewNotifyClicked();
	void AddNewNotify(const FText& NewNotifyName, ETextCommit::Type CommitInfo);

	// Trigger weight functions
	void OnSetTriggerWeightNotifyClicked(int32 NotifyIndex);
	void SetTriggerWeight(const FText& TriggerWeight, ETextCommit::Type CommitInfo, int32 NotifyIndex);

	// "Replace with... " commands
	void ReplaceSelectedWithBlueprintNotify(FString NewNotifyName, FString BlueprintPath);
	void ReplaceSelectedWithNotify(FString NewNotifyName, UClass* NotifyClass);
	bool IsValidToPlace(UClass* NotifyClass) const;

	// Whether we have one node selected
	bool IsSingleNodeSelected();
	// Checks the clipboard for an anim notify buffer, and returns whether there's only one notify
	bool IsSingleNodeInClipboard();

	/** Function to check whether it is possible to paste anim notify event */
	bool CanPasteAnimNotify() const;

	/** Handler for context menu paste command */
	void OnPasteNotifyClicked(ETagPasteMode::Type PasteMode, ETagPasteMultipleMode::Type MultiplePasteType = ETagPasteMultipleMode::Absolute);

	/** Handler for popup window asking the user for a paste time */
	void OnPasteNotifyTimeSet(const FText& TimeText, ETextCommit::Type CommitInfo);

	/** Function to paste a previously copied notify */
	void OnPasteNotify(float TimeToPasteAt, ETagPasteMultipleMode::Type MultiplePasteType = ETagPasteMultipleMode::Absolute);

	/** Provides direct access to the notify menu from the context menu */
	void OnManageNotifies();

	/** Opens the supplied blueprint in an editor */
	void OnOpenNotifySource(UBlueprint* InSourceBlueprint) const;

	/** Filters the asset browser by the selected notify */
	void OnFilterSkeletonNotify(FName InName);

	/**
	 * Selects a node on the track. Supports multi selection
	 * @param TrackNodeIndex - Index of the node to select.
	 * @param Append - Whether to append to to current selection or start a new one.
	 * @param bUpdateSelection - Whether to immediately inform Persona of a selection change
	 */
	void SelectTrackObjectNode(int32 TrackNodeIndex, bool Append, bool bUpdateSelection = true);

	/**
	 * Toggles the selection status of a notify node, for example when
	 * Control is held when clicking.
	 * @param NotifyIndex - Index of the notify to toggle the selection status of
	 * @param bUpdateSelection - Whether to immediately inform Persona of a selection change
	 */
	void ToggleTrackObjectNodeSelectionStatus(int32 TrackNodeIndex, bool bUpdateSelection = true);

	/**
	 * Deselects requested notify node
	 * @param NotifyIndex - Index of the notify node to deselect
	 * @param bUpdateSelection - Whether to immediately inform Persona of a selection change
	 */
	void DeselectTrackObjectNode(int32 TrackNodeIndex, bool bUpdateSelection = true);

	int32 GetHitNotifyNode(const FGeometry& MyGeometry, const FVector2D& Position);

	TSharedPtr<SWidget> SummonContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	float CalculateTime(const FGeometry& MyGeometry, FVector2D NodePos, bool bInputIsAbsolute = true);

	// Handler that is called when the user starts dragging a node
	FReply OnTagNodeDragStarted(TSharedRef<SMotionTagNode> NotifyNode, const FPointerEvent& MouseEvent, const FVector2D& ScreenNodePosition, const bool bDragOnMarker, int32 NotifyIndex);

	const EVisibility GetTimingNodeVisibility(TSharedPtr<SMotionTagNode> NotifyNode);

private:

	// Data structure for bluprint notify context menu entries
	struct BlueprintNotifyMenuInfo
	{
		FString NotifyName;
		FString BlueprintPath;
		UClass* BaseClass;
	};

	// Store the tracks geometry for later use
	void UpdateCachedGeometry(const FGeometry& InGeometry);

	// Returns the padding needed to render the notify in the correct track position
	FMargin GetNotifyTrackPadding(int32 NotifyIndex) const
	{
		float LeftMargin = NotifyPairs[NotifyIndex]->GetWidgetPaddingLeft();
		float RightMargin = CachedGeometry.GetLocalSize().X - NotifyNodes[NotifyIndex]->GetWidgetPosition().X - NotifyNodes[NotifyIndex]->GetSize().X;
		return FMargin(LeftMargin, 0, RightMargin, 0);
	}

	// Builds a UObject selection set and calls the OnSelectionChanged delegate
	void SendSelectionChanged()
	{
		OnSelectionChanged.ExecuteIfBound();
	}

protected:
	TWeakPtr<FUICommandList> WeakCommandList;

	float LastClickedTime;

	TObjectPtr<UMotionAnimObject> MotionAnim; // need for menu generation of anim notifies - 
	TArray<TSharedPtr<SMotionTagNode>>		NotifyNodes;
	TArray<TSharedPtr<SMotionTagPair>>		NotifyPairs;
	TArray<FAnimNotifyEvent*>				AnimNotifies;
	TAttribute<float>						ViewInputMin;
	TAttribute<float>						ViewInputMax;
	TAttribute<float>						InputMin;
	TAttribute<float>						InputMax;
	TAttribute<FLinearColor>				TrackColor;
	int32									TrackIndex;
	TAttribute<EVisibility>					NotifyTimingNodeVisibility;
	TAttribute<EVisibility>					BranchingPointTimingNodeVisibility;
	FOnTrackSelectionChanged				OnSelectionChanged;
	FOnUpdatePanel							OnUpdatePanel;
	FOnGetBlueprintTagData				OnGetTagBlueprintData;
	FOnGetBlueprintTagData				OnGetTagStateBlueprintData;
	FOnGetNativeTagClasses				OnGetTagNativeClasses;
	FOnGetNativeTagClasses				OnGetTagStateNativeClasses;
	FOnGetScrubValue						OnGetScrubValue;
	FOnGetDraggedNodePos					OnGetDraggedNodePos;
	FOnNotifyNodesDragStarted				OnNodeDragStarted;
	FPanTrackRequest						OnRequestTrackPan;
	FDeselectAllTags					OnDeselectAllTags;
	FCopyNodes								OnCopyNodes;
	FPasteNodes								OnPasteNodes;
	FOnSetInputViewRange					OnSetInputViewRange;
	FOnGetTimingNodeVisibility				OnGetTimingNodeVisibility;

	/** Delegate to call when offsets should be refreshed in a montage */
	FRefreshOffsetsRequest					OnRequestRefreshOffsets;

	/** Delegate to call when deleting notifies */
	FDeleteTag							OnDeleteTag;

	/** Delegates to call when replacing notifies */
	FOnGetIsMotionTagSelectionValidForReplacement OnGetIsMotionTagSelectionValidForReplacement;
	FReplaceWithTag						OnReplaceSelectedWithTag;
	FReplaceWithBlueprintTag				OnReplaceSelectedWithBlueprintTag;

	FOnInvokeTab							OnInvokeTab;

	TSharedPtr<SBorder>						TrackArea;

	/** Cache the SOverlay used to store all this tracks nodes */
	TSharedPtr<SOverlay> NodeSlots;

	/** Cached for drag drop handling code */
	FGeometry CachedGeometry;

	/** Delegate used to snap when dragging */
	FOnSnapPosition OnSnapPosition;

	/** Nodes that are currently selected */
	TArray<int32> SelectedNodeIndices;
};

//////////////////////////////////////////////////////////////////////////
// 

/** Widget for drawing a single track */
class STagEdTrack : public SCompoundWidget
{
private:
	/** Index of Track in Sequence **/
	int32 TrackIndex;

	/** Anim Sequence **/
	TObjectPtr<UMotionAnimObject> MotionAnim;

	/** Pointer to notify panel for drawing*/
	TWeakPtr<SMotionTagPanel> AnimPanelPtr;

public:
	SLATE_BEGIN_ARGS(STagEdTrack)
		: _TrackIndex(INDEX_NONE)
		, _AnimNotifyPanel()
		, _MotionAnim()
		, _WidgetWidth()
		, _ViewInputMin()
		, _ViewInputMax()
		, _OnSelectionChanged()
		, _OnUpdatePanel()
		, _OnDeleteTag()
		, _OnDeselectAllTags()
		, _OnCopyNodes()
		, _OnSetInputViewRange()
	{}
	SLATE_ARGUMENT(int32, TrackIndex)
		SLATE_ARGUMENT(TSharedPtr<SMotionTagPanel>, AnimNotifyPanel)
		SLATE_ARGUMENT(TObjectPtr<UMotionAnimObject>, MotionAnim)
		SLATE_ARGUMENT(float, WidgetWidth)
		SLATE_ATTRIBUTE(float, ViewInputMin)
		SLATE_ATTRIBUTE(float, ViewInputMax)
		SLATE_EVENT(FOnSnapPosition, OnSnapPosition)
		SLATE_ATTRIBUTE(EVisibility, NotifyTimingNodeVisibility)
		SLATE_ATTRIBUTE(EVisibility, BranchingPointTimingNodeVisibility)
		SLATE_EVENT(FOnTrackSelectionChanged, OnSelectionChanged)
		SLATE_EVENT(FOnGetScrubValue, OnGetScrubValue)
		SLATE_EVENT(FOnGetDraggedNodePos, OnGetDraggedNodePos)
		SLATE_EVENT(FOnUpdatePanel, OnUpdatePanel)
		SLATE_EVENT(FOnGetBlueprintTagData, OnGetTagBlueprintData)
		SLATE_EVENT(FOnGetBlueprintTagData, OnGetTagStateBlueprintData)
		SLATE_EVENT(FOnGetNativeTagClasses, OnGetTagNativeClasses)
		SLATE_EVENT(FOnGetNativeTagClasses, OnGetTagStateNativeClasses)
		SLATE_EVENT(FOnNotifyNodesDragStarted, OnNodeDragStarted)
		SLATE_EVENT(FRefreshOffsetsRequest, OnRequestRefreshOffsets)
		SLATE_EVENT(FDeleteTag, OnDeleteTag)
		SLATE_EVENT(FDeselectAllTags, OnDeselectAllTags)
		SLATE_EVENT(FCopyNodes, OnCopyNodes)
		SLATE_EVENT(FPasteNodes, OnPasteNodes)
		SLATE_EVENT(FOnSetInputViewRange, OnSetInputViewRange)
		SLATE_EVENT(FOnGetTimingNodeVisibility, OnGetTimingNodeVisibility)
		SLATE_EVENT(FOnInvokeTab, OnInvokeTab)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
	bool CanDeleteTrack();

	/** Pointer to actual anim notify track */
	TSharedPtr<class SMotionTagTrack>	NotifyTrack;

	/** Return the tracks name as an FText */
	FText GetTrackName() const
	{
		if (MotionAnim->MotionTagTracks.IsValidIndex(TrackIndex))
		{
			return FText::FromName(MotionAnim->MotionTagTracks[TrackIndex].TrackName);
		}

		/** Should never be possible but better than crashing the editor */
		return LOCTEXT("TrackName_Invalid", "Invalid Track");
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class FTagDragDropOp : public FDragDropOperation
{
public:
	FTagDragDropOp(float& InCurrentDragXPosition) :
		CurrentDragXPosition(InCurrentDragXPosition),
		SnapTime(-1.f),
		SelectionTimeLength(0.0f)
	{
	}

	struct FTrackClampInfo
	{
		int32 TrackPos;
		int32 TrackSnapTestPos;
		TSharedPtr<SMotionTagTrack> NotifyTrack;
	};

	DRAG_DROP_OPERATOR_TYPE(FTagDragDropOp, FDragDropOperation)

	virtual void OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent) override
	{
		if (bDropWasHandled == false)
		{
			int32 NumNodes = SelectedNodes.Num();

			const FScopedTransaction Transaction(NumNodes > 0 ? LOCTEXT("MoveTagEvent", "Move Motion Tags") : LOCTEXT("MoveTagsEvent", "Move Motion Tag"));

			MotionAnim->Modify();
			
			for (int32 CurrentNode = 0; CurrentNode < NumNodes; ++CurrentNode)
			{
				TSharedPtr<SMotionTagNode> Node = SelectedNodes[CurrentNode];
				float NodePositionOffset = NodeXOffsets[CurrentNode];
				const FTrackClampInfo& ClampInfo = GetTrackClampInfo(Node->GetScreenPosition());
				ClampInfo.NotifyTrack->HandleNodeDrop(Node, NodePositionOffset);
				Node->DropCancelled();
			}

			OnUpdatePanel.ExecuteIfBound();
		}

		FDragDropOperation::OnDrop(bDropWasHandled, MouseEvent);
	}

	virtual void OnDragged(const class FDragDropEvent& DragDropEvent) override
	{
		// Reset snapped node pointer
		SnappedNode = NULL;

		NodeGroupPosition = DragDropEvent.GetScreenSpacePosition() + DragOffset;

		FTrackClampInfo* SelectionPositionClampInfo = &GetTrackClampInfo(DragDropEvent.GetScreenSpacePosition());
		if ((SelectionPositionClampInfo->NotifyTrack->GetTrackIndex() + TrackSpan) >= ClampInfos.Num())
		{
			// Our selection has moved off the bottom of the notify panel, adjust the clamping information to keep it on the panel
			SelectionPositionClampInfo = &ClampInfos[ClampInfos.Num() - TrackSpan - 1];
		}

		const FGeometry& TrackGeom = SelectionPositionClampInfo->NotifyTrack->GetCachedGeometry();
		const FTrackScaleInfo& TrackScaleInfo = SelectionPositionClampInfo->NotifyTrack->GetCachedScaleInfo();

		FVector2D SelectionBeginPosition = TrackGeom.LocalToAbsolute(TrackGeom.AbsoluteToLocal(NodeGroupPosition) + SelectedNodes[0]->GetNotifyPositionOffset());

		float LocalTrackMin = TrackScaleInfo.InputToLocalX(0.0f);
		float LocalTrackMax = TrackScaleInfo.InputToLocalX(MotionAnim->GetPlayLength());
		float LocalTrackWidth = LocalTrackMax - LocalTrackMin;

		// Tracks the movement amount to apply to the selection due to a snap.
		float SnapMovement = 0.0f;
		// Clamp the selection into the track
		float SelectionBeginLocalPositionX = TrackGeom.AbsoluteToLocal(SelectionBeginPosition).X;
		const float ClampedEnd = FMath::Clamp(SelectionBeginLocalPositionX + NodeGroupSize.X, LocalTrackMin, LocalTrackMax);
		const float ClampedBegin = FMath::Clamp(SelectionBeginLocalPositionX, LocalTrackMin, LocalTrackMax);
		if (ClampedBegin > SelectionBeginLocalPositionX)
		{
			SelectionBeginLocalPositionX = ClampedBegin;
		}
		else if (ClampedEnd < SelectionBeginLocalPositionX + NodeGroupSize.X)
		{
			SelectionBeginLocalPositionX = ClampedEnd - NodeGroupSize.X;
		}

		SelectionBeginPosition.X = TrackGeom.LocalToAbsolute(FVector2D(SelectionBeginLocalPositionX, 0.0f)).X;

		// Handle node snaps
		bool bSnapped = false;
		for (int32 NodeIdx = 0; NodeIdx < SelectedNodes.Num() && !bSnapped; ++NodeIdx)
		{
			TSharedPtr<SMotionTagNode> CurrentNode = SelectedNodes[NodeIdx];

			// Clear off any snap time currently stored
			CurrentNode->ClearLastSnappedTime();

			const FTrackClampInfo& NodeClamp = GetTrackClampInfo(CurrentNode->GetScreenPosition());

			FVector2D EventPosition = SelectionBeginPosition + FVector2D(TrackScaleInfo.PixelsPerInput * NodeTimeOffsets[NodeIdx], 0.0f);

			// Look for a snap on the first scrub handle
			FVector2D TrackNodePos = TrackGeom.AbsoluteToLocal(EventPosition);
			const FVector2D OriginalNodePosition = TrackNodePos;
			float SequenceEnd = TrackScaleInfo.InputToLocalX(MotionAnim->GetPlayLength());

			// Always clamp the Y to the current track
			SelectionBeginPosition.Y = SelectionPositionClampInfo->TrackPos - 1.0f;

			float SnapX = GetSnapPosition(NodeClamp, TrackNodePos.X, bSnapped);
			if (FAnimNotifyEvent* CurrentEvent = CurrentNode->NodeObjectInterface->GetNotifyEvent())
			{
				if (bSnapped)
				{
					EAnimEventTriggerOffsets::Type Offset = EAnimEventTriggerOffsets::NoOffset;
					if (SnapX == 0.0f || SnapX == SequenceEnd)
					{
						Offset = SnapX > 0.0f ? EAnimEventTriggerOffsets::OffsetBefore : EAnimEventTriggerOffsets::OffsetAfter;
					}
					else
					{
						Offset = (SnapX < TrackNodePos.X) ? EAnimEventTriggerOffsets::OffsetAfter : EAnimEventTriggerOffsets::OffsetBefore;
					}

					CurrentEvent->TriggerTimeOffset = GetTriggerTimeOffsetForType(Offset);
					CurrentNode->SetLastSnappedTime(TrackScaleInfo.LocalXToInput(SnapX));

					if (SnapMovement == 0.0f)
					{
						SnapMovement = SnapX - TrackNodePos.X;
						TrackNodePos.X = SnapX;
						SnapTime = TrackScaleInfo.LocalXToInput(SnapX);
						SnappedNode = CurrentNode;
					}
					EventPosition = NodeClamp.NotifyTrack->GetCachedGeometry().LocalToAbsolute(TrackNodePos);
				}
				else
				{
					CurrentEvent->TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::NoOffset);
				}

				if (CurrentNode.IsValid() && CurrentEvent->GetDuration() > 0)
				{
					// If we didn't snap the beginning of the node, attempt to snap the end
					if (!bSnapped)
					{
						FVector2D TrackNodeEndPos = TrackNodePos + CurrentNode->GetDurationSize();
						SnapX = GetSnapPosition(*SelectionPositionClampInfo, TrackNodeEndPos.X, bSnapped);

						// Only attempt to snap if the node will fit on the track
						if (SnapX >= CurrentNode->GetDurationSize())
						{
							EAnimEventTriggerOffsets::Type Offset = EAnimEventTriggerOffsets::NoOffset;
							if (SnapX == SequenceEnd)
							{
								// Only need to check the end of the sequence here; end handle can't hit the beginning
								Offset = EAnimEventTriggerOffsets::OffsetBefore;
							}
							else
							{
								Offset = (SnapX < TrackNodeEndPos.X) ? EAnimEventTriggerOffsets::OffsetAfter : EAnimEventTriggerOffsets::OffsetBefore;
							}
							CurrentEvent->EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Offset);

							if (SnapMovement == 0.0f)
							{
								SnapMovement = SnapX - TrackNodeEndPos.X;
								SnapTime = TrackScaleInfo.LocalXToInput(SnapX) - CurrentEvent->GetDuration();
								CurrentNode->SetLastSnappedTime(SnapTime);
								SnappedNode = CurrentNode;
							}
						}
						else
						{
							// Remove any trigger time if we can't fit the node in.
							CurrentEvent->EndTriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::NoOffset);
						}
					}
				}
			}
		}

		SelectionBeginPosition.X += SnapMovement;

		CurrentDragXPosition = TrackGeom.AbsoluteToLocal(FVector2D(SelectionBeginPosition.X, 0.0f)).X;

		CursorDecoratorWindow->MoveWindowTo(TrackGeom.LocalToAbsolute(TrackGeom.AbsoluteToLocal(SelectionBeginPosition) - SelectedNodes[0]->GetNotifyPositionOffset()));
		NodeGroupPosition = SelectionBeginPosition;

		//scroll view
		float LocalMouseXPos = TrackGeom.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition()).X;
		float LocalViewportMin = 0.0f;
		float LocalViewportMax = TrackGeom.GetLocalSize().X;
		if (LocalMouseXPos < LocalViewportMin && LocalViewportMin > LocalTrackMin - 10.0f)
		{
			float ScreenDelta = FMath::Max(LocalMouseXPos - LocalViewportMin, -10.0f);
			RequestTrackPan.Execute(ScreenDelta, FVector2D(LocalTrackWidth, 1.f));
		}
		else if (LocalMouseXPos > LocalViewportMax && LocalViewportMax < LocalTrackMax + 10.0f)
		{
			float ScreenDelta = FMath::Max(LocalMouseXPos - LocalViewportMax, 10.0f);
			RequestTrackPan.Execute(ScreenDelta, FVector2D(LocalTrackWidth, 1.f));
		}
	}

	float GetSnapPosition(const FTrackClampInfo& ClampInfo, float WidgetSpaceNotifyPosition, bool& bOutSnapped)
	{
		const FTrackScaleInfo& ScaleInfo = ClampInfo.NotifyTrack->GetCachedScaleInfo();

		const float MaxSnapDist = 5.f;

		float CurrentMinSnapDest = MaxSnapDist;
		float SnapPosition = ScaleInfo.LocalXToInput(WidgetSpaceNotifyPosition);
		bOutSnapped = OnSnapPosition.IsBound() && !FSlateApplication::Get().GetModifierKeys().IsControlDown() && OnSnapPosition.Execute(SnapPosition, MaxSnapDist / ScaleInfo.PixelsPerInput, TArrayView<const FName>());
		SnapPosition = ScaleInfo.InputToLocalX(SnapPosition);

		float WidgetSpaceStartPosition = ScaleInfo.InputToLocalX(0.0f);
		float WidgetSpaceEndPosition = ScaleInfo.InputToLocalX(MotionAnim->GetPlayLength());

		if (!bOutSnapped)
		{
			// Didn't snap to a bar, snap to the track bounds
			float SnapDistBegin = FMath::Abs(WidgetSpaceStartPosition - WidgetSpaceNotifyPosition);
			float SnapDistEnd = FMath::Abs(WidgetSpaceEndPosition - WidgetSpaceNotifyPosition);
			if (SnapDistBegin < CurrentMinSnapDest)
			{
				SnapPosition = WidgetSpaceStartPosition;
				bOutSnapped = true;
			}
			else if (SnapDistEnd < CurrentMinSnapDest)
			{
				SnapPosition = WidgetSpaceEndPosition;
				bOutSnapped = true;
			}
		}

		return SnapPosition;
	}

	FTrackClampInfo& GetTrackClampInfo(const FVector2D NodePos)
	{
		int32 ClampInfoIndex = 0;
		int32 SmallestNodeTrackDist = FMath::Abs(ClampInfos[0].TrackSnapTestPos - NodePos.Y);
		for (int32 i = 0; i < ClampInfos.Num(); ++i)
		{
			int32 Dist = FMath::Abs(ClampInfos[i].TrackSnapTestPos - NodePos.Y);
			if (Dist < SmallestNodeTrackDist)
			{
				SmallestNodeTrackDist = Dist;
				ClampInfoIndex = i;
			}
		}
		return ClampInfos[ClampInfoIndex];
	}

	//class UAnimSequenceBase*			Sequence;				// The owning anim sequence
	TObjectPtr<UMotionAnimObject>					MotionAnim;				//The owning motionanimasset
	FVector2D							DragOffset;				// Offset from the mouse to place the decorator
	TArray<FTrackClampInfo>				ClampInfos;				// Clamping information for all of the available tracks
	float& CurrentDragXPosition;	// Current X position of the drag operation
	FPanTrackRequest					RequestTrackPan;		// Delegate to request a pan along the edges of a zoomed track
	TArray<float>						NodeTimes;				// Times to drop each selected node at
	float								SnapTime;				// The time that the snapped node was snapped to
	TWeakPtr<SMotionTagNode>			SnappedNode;			// The node chosen for the snap
	TArray<TSharedPtr<SMotionTagNode>> SelectedNodes;			// The nodes that are in the current selection
	TArray<float>						NodeTimeOffsets;		// Time offsets from the beginning of the selection to the nodes.
	TArray<float>						NodeXOffsets;			// Offsets in X from the widget position to the scrub handle for each node.
	FVector2D							NodeGroupPosition;		// Position of the beginning of the selection
	FVector2D							NodeGroupSize;			// Size of the entire selection
	TSharedPtr<SWidget>					Decorator;				// The widget to display when dragging
	float								SelectionTimeLength;	// Length of time that the selection covers
	int32								TrackSpan;				// Number of tracks that the selection spans
	FOnUpdatePanel						OnUpdatePanel;			// Delegate to redraw the notify panel
	FOnSnapPosition						OnSnapPosition;			// Delegate used to snap times

	static TSharedRef<FTagDragDropOp> New(
		TArray<TSharedPtr<SMotionTagNode>>			NotifyNodes,
		TSharedPtr<SWidget>							Decorator,
		const TArray<TSharedPtr<SMotionTagTrack>>& NotifyTracks,
		TObjectPtr<UMotionAnimObject> InMotionAnim,
		const FVector2D& CursorPosition,
		const FVector2D& SelectionScreenPosition,
		const FVector2D& SelectionSize,
		float& CurrentDragXPosition,
		FPanTrackRequest& RequestTrackPanDelegate,
		FOnSnapPosition& OnSnapPosition,
		FOnUpdatePanel& UpdatePanel
	)
	{
		TSharedRef<FTagDragDropOp> Operation = MakeShareable(new FTagDragDropOp(CurrentDragXPosition));
		Operation->MotionAnim = InMotionAnim;
		Operation->RequestTrackPan = RequestTrackPanDelegate;
		Operation->OnUpdatePanel = UpdatePanel;

		Operation->NodeGroupPosition = SelectionScreenPosition;
		Operation->NodeGroupSize = SelectionSize;
		Operation->DragOffset = SelectionScreenPosition - CursorPosition;
		Operation->OnSnapPosition = OnSnapPosition;
		Operation->Decorator = Decorator;
		Operation->SelectedNodes = NotifyNodes;
		Operation->TrackSpan = NotifyNodes.Last()->NodeObjectInterface->GetTrackIndex() - NotifyNodes[0]->NodeObjectInterface->GetTrackIndex();

		// Caclulate offsets for the selected nodes
		float BeginTime = MAX_flt;
		for (TSharedPtr<SMotionTagNode> Node : NotifyNodes)
		{
			float NotifyTime = Node->NodeObjectInterface->GetTime();

			if (NotifyTime < BeginTime)
			{
				BeginTime = NotifyTime;
			}
		}

		// Initialise node data
		for (TSharedPtr<SMotionTagNode> Node : NotifyNodes)
		{
			float NotifyTime = Node->NodeObjectInterface->GetTime();

			Node->ClearLastSnappedTime();
			Operation->NodeTimeOffsets.Add(NotifyTime - BeginTime);
			Operation->NodeTimes.Add(NotifyTime);
			Operation->NodeXOffsets.Add(Node->GetNotifyPositionOffset().X);

			// Calculate the time length of the selection. Because it is possible to have states
			// with arbitrary durations we need to search all of the nodes and find the furthest
			// possible point
			Operation->SelectionTimeLength = FMath::Max(Operation->SelectionTimeLength, NotifyTime + Node->NodeObjectInterface->GetDuration() - BeginTime);
		}

		Operation->Construct();

		for (int32 i = 0; i < NotifyTracks.Num(); ++i)
		{
			FTrackClampInfo Info;
			Info.NotifyTrack = NotifyTracks[i];
			const FGeometry& CachedGeometry = Info.NotifyTrack->GetCachedGeometry();
			Info.TrackPos = CachedGeometry.AbsolutePosition.Y;
			Info.TrackSnapTestPos = Info.TrackPos + (CachedGeometry.Size.Y / 2);
			Operation->ClampInfos.Add(Info);
		}

		Operation->CursorDecoratorWindow->SetOpacity(0.5f);
		return Operation;
	}

	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return Decorator;
	}

	FText GetHoverText() const
	{
		FText HoverText = LOCTEXT("Invalid", "Invalid");

		if (SelectedNodes[0].IsValid())
		{
			HoverText = FText::FromName(SelectedNodes[0]->NodeObjectInterface->GetName());
		}

		return HoverText;
	}
};

//////////////////////////////////////////////////////////////////////////
// SMotionTagNode

const float SMotionTagNode::MinimumStateDuration = (1.0f / 30.0f);

void SMotionTagNode::Construct(const FArguments& InArgs)
{
	//Sequence = InArgs._Sequence;
	MotionAnim = InArgs._MotionAnim;
	Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	bBeingDragged = false;
	CurrentDragHandle = ETagStateHandleHit::None;
	bDrawTooltipToRight = true;
	bSelected = false;
	DragMarkerTransactionIdx = INDEX_NONE;


	if (InArgs._AnimNotify)
	{
		MakeNodeInterface<FTagNodeInterface>(InArgs._AnimNotify);
	}
	else
	{
		check(false);	// Must specify something for this node to represent
						// Either AnimNotify or AnimSyncMarker
	}
	// Cache notify name for blueprint / Native notifies.
	NodeObjectInterface->CacheName();

	OnNodeDragStarted = InArgs._OnNodeDragStarted;
	PanTrackRequest = InArgs._PanTrackRequest;
	OnSelectionChanged = InArgs._OnSelectionChanged;
	OnUpdatePanel = InArgs._OnUpdatePanel;

	ViewInputMin = InArgs._ViewInputMin;
	ViewInputMax = InArgs._ViewInputMax;
	OnSnapPosition = InArgs._OnSnapPosition;

	if (InArgs._StateEndTimingNode.IsValid())
	{
		// The overlay will use the desired size to calculate the notify node size,
		// compute that once here.
		InArgs._StateEndTimingNode->SlatePrepass(1.0f);
		SAssignNew(EndMarkerNodeOverlay, SOverlay)
			+ SOverlay::Slot()
			[
				InArgs._StateEndTimingNode.ToSharedRef()
			];
	}

	SetClipping(EWidgetClipping::ClipToBounds);

	SetToolTipText(TAttribute<FText>(this, &SMotionTagNode::GetNodeTooltip));
}

FReply SMotionTagNode::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FVector2D ScreenNodePosition = FVector2D(MyGeometry.AbsolutePosition.X, MyGeometry.AbsolutePosition.Y);

	// Whether the drag has hit a duration marker
	bool bDragOnMarker = false;
	bBeingDragged = true;

	if (GetDurationSize() > 0.0f)
	{
		// This is a state node, check for a drag on the markers before movement. Use last screen space position before the drag started
		// as using the last position in the mouse event gives us a mouse position after the drag was started.
		ETagStateHandleHit::Type MarkerHit = DurationHandleHitTest(LastMouseDownPosition);
		if (MarkerHit == ETagStateHandleHit::Start || MarkerHit == ETagStateHandleHit::End)
		{
			bDragOnMarker = true;
			bBeingDragged = false;
			CurrentDragHandle = MarkerHit;

			MotionAnim->Modify();

			// Modify the owning sequence as we're now dragging the marker and begin a transaction
			check(DragMarkerTransactionIdx == INDEX_NONE);
			DragMarkerTransactionIdx = GEditor->BeginTransaction(NSLOCTEXT("AnimNotifyNode", "StateNodeDragTransation", "Drag State Node Marker"));
		}
	}

	return OnNodeDragStarted.Execute(SharedThis(this), MouseEvent, ScreenNodePosition, bDragOnMarker);
}

FLinearColor SMotionTagNode::GetNotifyColor() const
{
	TOptional<FLinearColor> Color = NodeObjectInterface->GetEditorColor();
	FLinearColor BaseColor = Color.Get(FLinearColor(1, 1, 0.5f));

	BaseColor.A = 0.67f;

	return BaseColor;
}

FText SMotionTagNode::GetNotifyText() const
{
	// Combine comment from notify struct and from function on object
	return FText::FromName(NodeObjectInterface->GetName());
}

FText SMotionTagNode::GetNodeTooltip() const
{
	return NodeObjectInterface->GetNodeTooltip(MotionAnim);
}

/** @return the Node's position within the graph */
UObject* SMotionTagNode::GetObjectBeingDisplayed() const
{
	TOptional<UObject*> Object = NodeObjectInterface->GetObjectBeingDisplayed();
	return Object.Get(Cast<UAnimSequenceBase>(MotionAnim->AnimAsset));
}

void SMotionTagNode::DropCancelled()
{
	bBeingDragged = false;
}

FVector2D SMotionTagNode::ComputeDesiredSize(float) const
{
	return GetSize();
}

bool SMotionTagNode::HitTest(const FGeometry& AllottedGeometry, FVector2D MouseLocalPose) const
{
	FVector2D Position = GetWidgetPosition();
	FVector2D Size = GetSize();

	return MouseLocalPose.ComponentwiseAllGreaterOrEqual(Position) && MouseLocalPose.ComponentwiseAllLessOrEqual(Position + Size);
}

ETagStateHandleHit::Type SMotionTagNode::DurationHandleHitTest(const FVector2D& CursorTrackPosition) const
{
	ETagStateHandleHit::Type MarkerHit = ETagStateHandleHit::None;

	// Make sure this node has a duration box (meaning it is a state node)
	if (NotifyDurationSizeX > 0.0f)
	{
		// Test for mouse inside duration box with handles included
		float ScrubHandleHalfWidth = ScrubHandleSize.X / 2.0f;

		// Position and size of the notify node including the scrub handles
		FVector2D NotifyNodePosition(NotifyScrubHandleCentre - ScrubHandleHalfWidth, 0.0f);
		FVector2D NotifyNodeSize(NotifyDurationSizeX + ScrubHandleHalfWidth * 2.0f, NotifyHeight);

		FVector2D MouseRelativePosition(CursorTrackPosition - GetWidgetPosition());

		if (MouseRelativePosition.ComponentwiseAllGreaterThan(NotifyNodePosition)
			&& MouseRelativePosition.ComponentwiseAllLessThan(NotifyNodePosition + NotifyNodeSize))
		{
			// Definitely inside the duration box, need to see which handle we hit if any
			if (MouseRelativePosition.X <= (NotifyNodePosition.X + ScrubHandleSize.X))
			{
				// Left Handle
				MarkerHit = ETagStateHandleHit::Start;
			}
			else if (MouseRelativePosition.X >= (NotifyNodePosition.X + NotifyNodeSize.X - ScrubHandleSize.X))
			{
				// Right Handle
				MarkerHit = ETagStateHandleHit::End;
			}
		}
	}

	return MarkerHit;
}

void SMotionTagNode::UpdateSizeAndPosition(const FGeometry& AllottedGeometry)
{
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0, 0, AllottedGeometry.Size);

	// Cache the geometry information, the alloted geometry is the same size as the track.
	CachedAllotedGeometrySize = AllottedGeometry.Size * AllottedGeometry.Scale;

	NotifyTimePositionX = ScaleInfo.InputToLocalX(NodeObjectInterface->GetTime());
	NotifyDurationSizeX = ScaleInfo.PixelsPerInput * NodeObjectInterface->GetDuration();

	const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	TextSize = FontMeasureService->Measure(GetNotifyText(), Font);
	LabelWidth = TextSize.X + (TextBorderSize.X * 2.f) + (ScrubHandleSize.X / 2.f);

	bool bDrawBranchingPoint = NodeObjectInterface->IsBranchingPoint();
	BranchingPointIconSize = FVector2D(TextSize.Y, TextSize.Y);
	if (bDrawBranchingPoint)
	{
		LabelWidth += BranchingPointIconSize.X + TextBorderSize.X * 2.f;
	}

	//Calculate scrub handle box size (the notional box around the scrub handle and the alignment marker)
	float NotifyHandleBoxWidth = FMath::Max(ScrubHandleSize.X, AlignmentMarkerSize.X * 2);

	// Work out where we will have to draw the tool tip
	FVector2D Size = GetSize();
	float LeftEdgeToNotify = NotifyTimePositionX;
	float RightEdgeToNotify = AllottedGeometry.Size.X - NotifyTimePositionX;
	bDrawTooltipToRight = NotifyDurationSizeX > 0.0f || ((RightEdgeToNotify > LabelWidth) || (RightEdgeToNotify > LeftEdgeToNotify));

	// Calculate widget width/position based on where we are drawing the tool tip
	WidgetX = bDrawTooltipToRight ? (NotifyTimePositionX - (NotifyHandleBoxWidth / 2.f)) : (NotifyTimePositionX - LabelWidth);
	WidgetSize = bDrawTooltipToRight ? FVector2D((NotifyDurationSizeX > 0.0f ? NotifyDurationSizeX : FMath::Max(LabelWidth, NotifyDurationSizeX)), NotifyHeight) : FVector2D((LabelWidth + NotifyDurationSizeX), NotifyHeight);
	WidgetSize.X += NotifyHandleBoxWidth;

	if (EndMarkerNodeOverlay.IsValid())
	{
		FVector2D OverlaySize = EndMarkerNodeOverlay->GetDesiredSize();
		WidgetSize.X += OverlaySize.X;
	}

	// Widget position of the notify marker
	NotifyScrubHandleCentre = bDrawTooltipToRight ? NotifyHandleBoxWidth / 2.f : LabelWidth;
}

/** @return the Node's position within the track */
FVector2D SMotionTagNode::GetWidgetPosition() const
{
	return FVector2D(WidgetX, NotifyHeightOffset);
}

FVector2D SMotionTagNode::GetNotifyPosition() const
{
	return FVector2D(NotifyTimePositionX, NotifyHeightOffset);
}

FVector2D SMotionTagNode::GetNotifyPositionOffset() const
{
	return GetNotifyPosition() - GetWidgetPosition();
}

FVector2D SMotionTagNode::GetSize() const
{
	return WidgetSize;
}

int32 SMotionTagNode::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 MarkerLayer = LayerId + 1;
	int32 ScrubHandleID = MarkerLayer + 1;
	int32 TextLayerID = ScrubHandleID + 1;
	int32 BranchPointLayerID = TextLayerID + 1;

	FAnimNotifyEvent* AnimNotifyEvent = NodeObjectInterface->GetNotifyEvent();

	// Paint marker node if we have one
	if (EndMarkerNodeOverlay.IsValid())
	{
		FVector2D MarkerSize = EndMarkerNodeOverlay->GetDesiredSize();
		FVector2D MarkerOffset(NotifyDurationSizeX + MarkerSize.X * 0.5f + 5.0f, (NotifyHeight - MarkerSize.Y) * 0.5f);
		EndMarkerNodeOverlay->Paint(Args.WithNewParent(this), AllottedGeometry.MakeChild(
			MarkerSize, FSlateLayoutTransform(1.0f, MarkerOffset)), MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	const FSlateBrush* StyleInfo = FAppStyle::GetBrush(TEXT("SpecialEditableTextImageNormal"));

	FText Text = GetNotifyText();
	FLinearColor NodeColor = SMotionTagNode::GetNotifyColor();
	FLinearColor BoxColor = bSelected ? FAppStyle::GetSlateColor("SelectionColor").GetSpecifiedColor() : SMotionTagNode::GetNotifyColor();

	float HalfScrubHandleWidth = ScrubHandleSize.X / 2.0f;

	// Show duration of AnimNotifyState
	if (NotifyDurationSizeX > 0.f)
	{
		FVector2D DurationBoxSize = FVector2D(NotifyDurationSizeX, TextSize.Y + TextBorderSize.Y * 2.f);
		FVector2D DurationBoxPosition = FVector2D(NotifyScrubHandleCentre, (NotifyHeight - TextSize.Y) * 0.5f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(DurationBoxSize, FSlateLayoutTransform(1.0f, DurationBoxPosition)),
			StyleInfo,
			ESlateDrawEffect::None,
			BoxColor);

		DrawScrubHandle(DurationBoxPosition.X + DurationBoxSize.X, OutDrawElements, ScrubHandleID, AllottedGeometry, MyCullingRect, NodeColor);

		// Render offsets if necessary
		if (AnimNotifyEvent && AnimNotifyEvent->EndTriggerTimeOffset != 0.f) //Do we have an offset to render?
		{
			float EndTime = AnimNotifyEvent->GetTime() + AnimNotifyEvent->GetDuration();
			if (EndTime != MotionAnim->GetPlayLength()) //Don't render offset when we are at the end of the sequence, doesnt help the user
			{
				// ScrubHandle
				float HandleCentre = NotifyDurationSizeX + (ScrubHandleSize.X - 2.0f);
				DrawHandleOffset(AnimNotifyEvent->EndTriggerTimeOffset, HandleCentre, OutDrawElements, MarkerLayer, AllottedGeometry, MyCullingRect, NodeColor);
			}
		}
	}

	// Branching point
	bool bDrawBranchingPoint = AnimNotifyEvent && AnimNotifyEvent->IsBranchingPoint();

	// Background
	FVector2D LabelSize = TextSize + TextBorderSize * 2.f;
	LabelSize.X += HalfScrubHandleWidth + (bDrawBranchingPoint ? (BranchingPointIconSize.X + TextBorderSize.X * 2.f) : 0.f);

	FVector2D LabelPosition(bDrawTooltipToRight ? NotifyScrubHandleCentre : NotifyScrubHandleCentre - LabelSize.X, (NotifyHeight - TextSize.Y) * 0.5f);

	if (NotifyDurationSizeX == 0.f)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(LabelSize, FSlateLayoutTransform(1.0f, LabelPosition)),
			StyleInfo,
			ESlateDrawEffect::None,
			BoxColor);
	}

	// Text
	FVector2D TextPosition = LabelPosition + TextBorderSize;
	if (bDrawTooltipToRight)
	{
		TextPosition.X += HalfScrubHandleWidth;
	}

	FVector2D DrawTextSize;
	DrawTextSize.X = (NotifyDurationSizeX > 0.0f ? FMath::Min(NotifyDurationSizeX - (ScrubHandleSize.X + (bDrawBranchingPoint ? BranchingPointIconSize.X : 0)), TextSize.X) : TextSize.X);
	DrawTextSize.Y = TextSize.Y;

	if (bDrawBranchingPoint)
	{
		TextPosition.X += BranchingPointIconSize.X;
	}

	FPaintGeometry TextGeometry = AllottedGeometry.ToPaintGeometry(DrawTextSize, FSlateLayoutTransform(1.0f, TextPosition));
	OutDrawElements.PushClip(FSlateClippingZone(TextGeometry));

	FSlateDrawElement::MakeText(
		OutDrawElements,
		TextLayerID,
		TextGeometry,
		Text,
		Font,
		ESlateDrawEffect::None,
		FLinearColor::Black
	);

	OutDrawElements.PopClip();

	// Draw Branching Point
	if (bDrawBranchingPoint)
	{
		FVector2D BranchPointIconPos = LabelPosition + TextBorderSize;
		if (bDrawTooltipToRight)
		{
			BranchPointIconPos.X += HalfScrubHandleWidth;
		}
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			BranchPointLayerID,
			AllottedGeometry.ToPaintGeometry(BranchingPointIconSize, FSlateLayoutTransform(1.0f, BranchPointIconPos)),
			FAppStyle::GetBrush(TEXT("AnimNotifyEditor.BranchingPoint")),
			ESlateDrawEffect::None,
			FLinearColor::White
		);
	}

	DrawScrubHandle(NotifyScrubHandleCentre, OutDrawElements, ScrubHandleID, AllottedGeometry, MyCullingRect, NodeColor);

	if (AnimNotifyEvent && AnimNotifyEvent->TriggerTimeOffset != 0.f) //Do we have an offset to render?
	{
		float NotifyTime = AnimNotifyEvent->GetTime();
		if (NotifyTime != 0.f && NotifyTime != MotionAnim->GetPlayLength()) //Don't render offset when we are at the start/end of the sequence, doesn't help the user
		{
			float HandleCentre = NotifyScrubHandleCentre;
			float& Offset = AnimNotifyEvent->TriggerTimeOffset;

			DrawHandleOffset(AnimNotifyEvent->TriggerTimeOffset, NotifyScrubHandleCentre, OutDrawElements, MarkerLayer, AllottedGeometry, MyCullingRect, NodeColor);
		}
	}

	return TextLayerID;
}

FReply SMotionTagNode::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Don't do scrub handle dragging if we haven't captured the mouse.
	if (!this->HasMouseCapture()) return FReply::Unhandled();

	if (CurrentDragHandle == ETagStateHandleHit::None)
	{
		// We've had focus taken away - realease the mouse
		FSlateApplication::Get().ReleaseAllPointerCapture();
		return FReply::Unhandled();
	}

	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0, 0, CachedAllotedGeometrySize);

	float XPositionInTrack = MyGeometry.AbsolutePosition.X - CachedTrackGeometry.AbsolutePosition.X;
	float TrackScreenSpaceXPosition = MyGeometry.AbsolutePosition.X - XPositionInTrack;
	float TrackScreenSpaceOrigin = CachedTrackGeometry.LocalToAbsolute(FVector2D(ScaleInfo.InputToLocalX(0.0f), 0.0f)).X;
	float TrackScreenSpaceLimit = CachedTrackGeometry.LocalToAbsolute(FVector2D(ScaleInfo.InputToLocalX(MotionAnim->GetPlayLength()), 0.0f)).X;

	if (CurrentDragHandle == ETagStateHandleHit::Start)
	{
		// Check track bounds
		float OldDisplayTime = NodeObjectInterface->GetTime();

		if (MouseEvent.GetScreenSpacePosition().X >= TrackScreenSpaceXPosition && MouseEvent.GetScreenSpacePosition().X <= TrackScreenSpaceXPosition + CachedAllotedGeometrySize.X)
		{
			const FVector2D GeometryAbsolutePosition(MyGeometry.AbsolutePosition.X, MyGeometry.AbsolutePosition.Y);
			float NewDisplayTime = ScaleInfo.LocalXToInput((MouseEvent.GetScreenSpacePosition() - GeometryAbsolutePosition + XPositionInTrack).X);
			float NewDuration = NodeObjectInterface->GetDuration() + OldDisplayTime - NewDisplayTime;

			// Check to make sure the duration is not less than the minimum allowed
			if (NewDuration < MinimumStateDuration)
			{
				NewDisplayTime -= MinimumStateDuration - NewDuration;
			}

			NodeObjectInterface->SetTime(FMath::Max(0.0f, NewDisplayTime));
			NodeObjectInterface->SetDuration(NodeObjectInterface->GetDuration() + OldDisplayTime - NodeObjectInterface->GetTime());
		}
		else if (NodeObjectInterface->GetDuration() > MinimumStateDuration)
		{
			float Overflow = HandleOverflowPan(MouseEvent.GetScreenSpacePosition(), TrackScreenSpaceXPosition, TrackScreenSpaceOrigin, TrackScreenSpaceLimit);

			// Update scale info to the new view inputs after panning
			ScaleInfo.ViewMinInput = ViewInputMin.Get();
			ScaleInfo.ViewMaxInput = ViewInputMax.Get();

			const FVector2D GeometryAbsolutePosition(MyGeometry.AbsolutePosition.X, MyGeometry.AbsolutePosition.Y);
			const float NewDisplayTime = ScaleInfo.LocalXToInput((MouseEvent.GetScreenSpacePosition() - GeometryAbsolutePosition + XPositionInTrack).X);
			NodeObjectInterface->SetTime(FMath::Max(0.0f, NewDisplayTime));
			NodeObjectInterface->SetDuration(NodeObjectInterface->GetDuration() + OldDisplayTime - NodeObjectInterface->GetTime());

			// Adjust incase we went under the minimum
			if (NodeObjectInterface->GetDuration() < MinimumStateDuration)
			{
				float EndTimeBefore = NodeObjectInterface->GetTime() + NodeObjectInterface->GetDuration();
				NodeObjectInterface->SetTime(NodeObjectInterface->GetTime() + NodeObjectInterface->GetDuration() - MinimumStateDuration);
				NodeObjectInterface->SetDuration(MinimumStateDuration);
				float EndTimeAfter = NodeObjectInterface->GetTime() + NodeObjectInterface->GetDuration();
			}
		}

		// Now we know where the marker should be, look for possible snaps on montage marker bars
		if (FAnimNotifyEvent* AnimNotifyEvent = NodeObjectInterface->GetNotifyEvent())
		{
			float InputStartTime = AnimNotifyEvent->GetTime();
			float MarkerSnap = GetScrubHandleSnapPosition(InputStartTime, ETagStateHandleHit::Start);
			if (MarkerSnap != -1.0f)
			{
				// We're near to a snap bar
				EAnimEventTriggerOffsets::Type Offset = (MarkerSnap < InputStartTime) ? EAnimEventTriggerOffsets::OffsetAfter : EAnimEventTriggerOffsets::OffsetBefore;
				AnimNotifyEvent->TriggerTimeOffset = GetTriggerTimeOffsetForType(Offset);

				// Adjust our start marker
				OldDisplayTime = AnimNotifyEvent->GetTime();
				AnimNotifyEvent->SetTime(MarkerSnap);
				AnimNotifyEvent->SetDuration(AnimNotifyEvent->GetDuration() + OldDisplayTime - AnimNotifyEvent->GetTime());
			}
			else
			{
				AnimNotifyEvent->TriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::NoOffset);
			}
		}
	}
	else
	{
		if (MouseEvent.GetScreenSpacePosition().X >= TrackScreenSpaceXPosition && MouseEvent.GetScreenSpacePosition().X <= TrackScreenSpaceXPosition + CachedAllotedGeometrySize.X)
		{
			const FVector2D GeometryAbsolutePosition(MyGeometry.AbsolutePosition.X, MyGeometry.AbsolutePosition.Y);
			const float NewDuration = ScaleInfo.LocalXToInput((MouseEvent.GetScreenSpacePosition() - GeometryAbsolutePosition + XPositionInTrack).X) - NodeObjectInterface->GetTime();

			NodeObjectInterface->SetDuration(FMath::Max(NewDuration, MinimumStateDuration));
		}
		else if (NodeObjectInterface->GetDuration() > MinimumStateDuration)
		{
			float Overflow = HandleOverflowPan(MouseEvent.GetScreenSpacePosition(), TrackScreenSpaceXPosition, TrackScreenSpaceOrigin, TrackScreenSpaceLimit);

			// Update scale info to the new view inputs after panning
			ScaleInfo.ViewMinInput = ViewInputMin.Get();
			ScaleInfo.ViewMaxInput = ViewInputMax.Get();

			const FVector2D GeometryAbsolutePosition(MyGeometry.AbsolutePosition.X, MyGeometry.AbsolutePosition.Y);
			const float NewDuration = ScaleInfo.LocalXToInput((MouseEvent.GetScreenSpacePosition() - GeometryAbsolutePosition + XPositionInTrack).X) - NodeObjectInterface->GetTime();
			NodeObjectInterface->SetDuration(FMath::Max(NewDuration, MinimumStateDuration));
		}

		if (NodeObjectInterface->GetTime() + NodeObjectInterface->GetDuration() > MotionAnim->GetPlayLength())
		{
			NodeObjectInterface->SetDuration(MotionAnim->GetPlayLength() - NodeObjectInterface->GetTime());
		}

		// Now we know where the scrub handle should be, look for possible snaps on montage marker bars
		if (FAnimNotifyEvent* AnimNotifyEvent = NodeObjectInterface->GetNotifyEvent())
		{
			float InputEndTime = AnimNotifyEvent->GetTime() + AnimNotifyEvent->GetDuration();
			float MarkerSnap = GetScrubHandleSnapPosition(InputEndTime, ETagStateHandleHit::End);
			if (MarkerSnap != -1.0f)
			{
				// We're near to a snap bar
				EAnimEventTriggerOffsets::Type Offset = (MarkerSnap < InputEndTime) ? EAnimEventTriggerOffsets::OffsetAfter : EAnimEventTriggerOffsets::OffsetBefore;
				AnimNotifyEvent->EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Offset);

				// Adjust our end marker
				AnimNotifyEvent->SetDuration(MarkerSnap - AnimNotifyEvent->GetTime());
			}
			else
			{
				AnimNotifyEvent->EndTriggerTimeOffset = GetTriggerTimeOffsetForType(EAnimEventTriggerOffsets::NoOffset);
			}
		}
	}

	return FReply::Handled();
}

FReply SMotionTagNode::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bool bLeftButton = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;

	if (bLeftButton && CurrentDragHandle != ETagStateHandleHit::None)
	{
		MotionAnim->Modify();
		
		// Clear the drag marker and give the mouse back
		CurrentDragHandle = ETagStateHandleHit::None;

		// Signal selection changing so details panels get updated
		OnSelectionChanged.ExecuteIfBound();

		// End drag transaction before handing mouse back
		check(DragMarkerTransactionIdx != INDEX_NONE);
		GEditor->EndTransaction();
		DragMarkerTransactionIdx = INDEX_NONE;

		OnUpdatePanel.ExecuteIfBound();

		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

float SMotionTagNode::GetScrubHandleSnapPosition(float NotifyInputX, ETagStateHandleHit::Type HandleToCheck)
{
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0, 0, CachedAllotedGeometrySize);

	const float MaxSnapDist = 5.0f;

	if (OnSnapPosition.IsBound() && !FSlateApplication::Get().GetModifierKeys().IsControlDown())
	{
		if (OnSnapPosition.Execute(NotifyInputX, MaxSnapDist / ScaleInfo.PixelsPerInput, TArrayView<const FName>()))
		{
			return NotifyInputX;
		}
	}

	return -1.0f;
}

FReply SMotionTagNode::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::SetDirectly, true);
}

float SMotionTagNode::HandleOverflowPan(const FVector2D& ScreenCursorPos, float TrackScreenSpaceXPosition, float TrackScreenSpaceMin, float TrackScreenSpaceMax)
{
	float Overflow = 0.0f;

	if (ScreenCursorPos.X < TrackScreenSpaceXPosition && TrackScreenSpaceXPosition > TrackScreenSpaceMin - 10.0f)
	{
		// Overflow left edge
		Overflow = FMath::Min(ScreenCursorPos.X - TrackScreenSpaceXPosition, -10.0f);
	}
	else if (ScreenCursorPos.X > CachedAllotedGeometrySize.X && (TrackScreenSpaceXPosition + CachedAllotedGeometrySize.X) < TrackScreenSpaceMax + 10.0f)
	{
		// Overflow right edge
		Overflow = FMath::Max(ScreenCursorPos.X - (TrackScreenSpaceXPosition + CachedAllotedGeometrySize.X), 10.0f);
	}

	PanTrackRequest.ExecuteIfBound(Overflow, CachedAllotedGeometrySize);

	return Overflow;
}

void SMotionTagNode::DrawScrubHandle(float ScrubHandleCentre, FSlateWindowElementList& OutDrawElements, int32 ScrubHandleID, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FLinearColor NodeColour) const
{
	FVector2D ScrubHandlePosition(ScrubHandleCentre - ScrubHandleSize.X / 2.0f, (NotifyHeight - ScrubHandleSize.Y) / 2.f);
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		ScrubHandleID,
		AllottedGeometry.ToPaintGeometry(ScrubHandleSize, FSlateLayoutTransform(1.0f, ScrubHandlePosition)),
		FAppStyle::GetBrush(TEXT("Sequencer.KeyDiamond")),
		ESlateDrawEffect::None,
		NodeColour
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		ScrubHandleID,
		AllottedGeometry.ToPaintGeometry(ScrubHandleSize, FSlateLayoutTransform(1.0f, ScrubHandlePosition)),
		FAppStyle::GetBrush(TEXT("Sequencer.KeyDiamondBorder")),
		ESlateDrawEffect::None,
		bSelected ? FAppStyle::GetSlateColor("SelectionColor").GetSpecifiedColor() : FLinearColor::Black
	);
}

void SMotionTagNode::DrawHandleOffset(const float& Offset, const float& HandleCentre, FSlateWindowElementList& OutDrawElements, int32 MarkerLayer, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FLinearColor NodeColor) const
{
	FVector2D MarkerPosition;
	FVector2D MarkerSize = AlignmentMarkerSize;

	if (Offset < 0.f)
	{
		MarkerPosition.Set(HandleCentre - AlignmentMarkerSize.X, (NotifyHeight - AlignmentMarkerSize.Y) / 2.f);
	}
	else
	{
		MarkerPosition.Set(HandleCentre + AlignmentMarkerSize.X, (NotifyHeight - AlignmentMarkerSize.Y) / 2.f);
		MarkerSize.X = -AlignmentMarkerSize.X;
	}

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		MarkerLayer,
		AllottedGeometry.ToPaintGeometry(MarkerSize, FSlateLayoutTransform(1.0f, MarkerPosition)),
		FAppStyle::GetBrush(TEXT("Sequencer.Timeline.NotifyAlignmentMarker")),
		ESlateDrawEffect::None,
		NodeColor
	);
}

void SMotionTagNode::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	ScreenPosition = FVector2D(AllottedGeometry.AbsolutePosition.X, AllottedGeometry.AbsolutePosition.Y);
}

void SMotionTagNode::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	if (CurrentDragHandle != ETagStateHandleHit::None)
	{
		// Lost focus while dragging a state node, clear the drag and end the current transaction
		CurrentDragHandle = ETagStateHandleHit::None;

		check(DragMarkerTransactionIdx != INDEX_NONE);
		GEditor->EndTransaction();
		DragMarkerTransactionIdx = INDEX_NONE;
	}
}

bool SMotionTagNode::SupportsKeyboardFocus() const
{
	// Need to support focus on the node so we can end drag transactions if the user alt-tabs
	// from the editor while in the proceess of dragging a state notify duration marker.
	return true;
}

FCursorReply SMotionTagNode::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	// Show resize cursor if the cursor is hoverring over either of the scrub handles of a notify state node
	if (IsHovered() && GetDurationSize() > 0.0f)
	{
		FVector2D RelMouseLocation = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition());

		const float HandleHalfWidth = ScrubHandleSize.X / 2.0f;
		const float DistFromFirstHandle = FMath::Abs(RelMouseLocation.X - NotifyScrubHandleCentre);
		const float DistFromSecondHandle = FMath::Abs(RelMouseLocation.X - (NotifyScrubHandleCentre + NotifyDurationSizeX));

		if (DistFromFirstHandle < HandleHalfWidth || DistFromSecondHandle < HandleHalfWidth || CurrentDragHandle != ETagStateHandleHit::None)
		{
			return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
		}
	}
	return FCursorReply::Unhandled();
}

//////////////////////////////////////////////////////////////////////////
// SMotionTagTrack

void SMotionTagTrack::Construct(const FArguments& InArgs)
{
	SetClipping(EWidgetClipping::ClipToBounds);

	WeakCommandList = InArgs._CommandList;
	//Sequence = InArgs._Sequence;
	MotionAnim = InArgs._MotionAnim;
	ViewInputMin = InArgs._ViewInputMin;
	ViewInputMax = InArgs._ViewInputMax;
	OnSelectionChanged = InArgs._OnSelectionChanged;
	AnimNotifies = InArgs._AnimNotifies;
	OnUpdatePanel = InArgs._OnUpdatePanel;
	OnGetTagBlueprintData = InArgs._OnGetTagBlueprintData;
	OnGetTagStateBlueprintData = InArgs._OnGetTagStateBlueprintData;
	OnGetTagNativeClasses = InArgs._OnGetTagNativeClasses;
	OnGetTagStateNativeClasses = InArgs._OnGetTagStateNativeClasses;
	TrackIndex = InArgs._TrackIndex;
	OnGetScrubValue = InArgs._OnGetScrubValue;
	OnGetDraggedNodePos = InArgs._OnGetDraggedNodePos;
	OnNodeDragStarted = InArgs._OnNodeDragStarted;
	TrackColor = InArgs._TrackColor;
	OnSnapPosition = InArgs._OnSnapPosition;
	OnRequestTrackPan = InArgs._OnRequestTrackPan;
	OnRequestRefreshOffsets = InArgs._OnRequestOffsetRefresh;
	OnDeleteTag = InArgs._OnDeleteTag;
	OnGetIsMotionTagSelectionValidForReplacement = InArgs._OnGetIsMotionTagSelectionValidForReplacement;
	OnReplaceSelectedWithTag = InArgs._OnReplaceSelectedWithTag;
	OnReplaceSelectedWithBlueprintTag = InArgs._OnReplaceSelectedWithBlueprintTag;
	OnDeselectAllTags = InArgs._OnDeselectAllTags;
	OnCopyNodes = InArgs._OnCopyNodes;
	OnPasteNodes = InArgs._OnPasteNodes;
	OnSetInputViewRange = InArgs._OnSetInputViewRange;
	OnGetTimingNodeVisibility = InArgs._OnGetTimingNodeVisibility;
	OnInvokeTab = InArgs._OnInvokeTab;

	this->ChildSlot
		[
			SAssignNew(TrackArea, SBorder)
			.Visibility(EVisibility::SelfHitTestInvisible)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		.Padding(FMargin(0.f, 0.f))
		];

	Update();
}

FVector2D SMotionTagTrack::ComputeDesiredSize(float) const
{
	FVector2D Size;
	Size.X = 200;
	Size.Y = FMotionTimelineTrack_TagsPanel::NotificationTrackHeight;
	return Size;
}

int32 SMotionTagTrack::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateBrush* StyleInfo = FAppStyle::GetBrush(TEXT("Persona.NotifyEditor.NotifyTrackBackground"));
	FLinearColor Color = TrackColor.Get();

	FPaintGeometry MyGeometry = AllottedGeometry.ToPaintGeometry();

	int32 CustomLayerId = LayerId + 1;
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0.f, 0.f, AllottedGeometry.Size);

	bool bAnyDraggedNodes = false;
	for (int32 I = 0; I < NotifyNodes.Num(); ++I)
	{
		if (NotifyNodes[I].Get()->bBeingDragged == false)
		{
			NotifyNodes[I].Get()->UpdateSizeAndPosition(AllottedGeometry);
		}
		else
		{
			bAnyDraggedNodes = true;
		}
	}

	if (TrackIndex < MotionAnim->MotionTagTracks.Num() - 1)
	{
		// Draw track bottom border
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			CustomLayerId,
			AllottedGeometry.ToPaintGeometry(),
			TArray<FVector2D>({ FVector2D(0.0f, AllottedGeometry.GetLocalSize().Y), FVector2D(AllottedGeometry.GetLocalSize().X, AllottedGeometry.GetLocalSize().Y) }),
			ESlateDrawEffect::None,
			FLinearColor(0.1f, 0.1f, 0.1f, 0.3f)
		);
	}

	++CustomLayerId;

	float Value = 0.f;

	if (bAnyDraggedNodes && OnGetDraggedNodePos.IsBound())
	{
		Value = OnGetDraggedNodePos.Execute();

		if (Value >= 0.0f)
		{
			float XPos = Value;
			TArray<FVector2D> LinePoints;
			LinePoints.Add(FVector2D(XPos, 0.f));
			LinePoints.Add(FVector2D(XPos, AllottedGeometry.Size.Y));

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				CustomLayerId,
				MyGeometry,
				LinePoints,
				ESlateDrawEffect::None,
				FLinearColor(1.0f, 0.5f, 0.0f)
			);
		}
	}

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, CustomLayerId, InWidgetStyle, bParentEnabled);
}

FCursorReply SMotionTagTrack::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (ViewInputMin.Get() > 0.f || ViewInputMax.Get() < MotionAnim->GetPlayLength())
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHand);
	}

	return FCursorReply::Unhandled();
}

template<typename NotifyTypeClass>
void SMotionTagTrack::MakeNewNotifyPicker(FMenuBuilder& MenuBuilder, bool bIsReplaceWithMenu /* = false */)
{
	FText TypeName = NotifyTypeClass::StaticClass() == UAnimNotify::StaticClass() ? LOCTEXT("MotionTagName", "Motion Tag") : LOCTEXT("AnimTagSectionName", "Motion Tag Section");
	FText SectionHeaderFormat = bIsReplaceWithMenu ? LOCTEXT("ReplaceWithAnExistingMotionTag", "Replace with an existing {0}") : LOCTEXT("AddsAnExistingMotionTag", "Add an existing {0}");

	class FNotifyStateClassFilter : public IClassViewerFilter
	{
	public:
		FNotifyStateClassFilter(TObjectPtr<UMotionAnimObject> InMotionAnim)
			: MotionAnim(InMotionAnim)
		{}

		bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			const bool bChildOfObjectClass = InClass->IsChildOf(NotifyTypeClass::StaticClass());
			const bool bMatchesFlags = !InClass->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract);
			return bChildOfObjectClass && bMatchesFlags && CastChecked<NotifyTypeClass>(InClass->ClassDefaultObject)->CanBePlaced(Cast<UAnimSequenceBase>(MotionAnim->AnimAsset));
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			const bool bChildOfObjectClass = InUnloadedClassData->IsChildOf(NotifyTypeClass::StaticClass());
			const bool bMatchesFlags = !InUnloadedClassData->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract);
			bool bValidToPlace = false;
			if (bChildOfObjectClass)
			{
				if (const UClass* NativeBaseClass = InUnloadedClassData->GetNativeParent())
				{
					bValidToPlace = CastChecked<NotifyTypeClass>(NativeBaseClass->ClassDefaultObject)->CanBePlaced(Cast<UAnimSequenceBase>(MotionAnim->AnimAsset));
				}
			}

			return bChildOfObjectClass && bMatchesFlags && bValidToPlace;
		}

		/** MotionAnim referenced by outer panel */
		TObjectPtr<UMotionAnimObject> MotionAnim;
	};

	// MenuBuilder always has a search widget added to it by default, hence if larger then 1 then something else has been added to it
	if (MenuBuilder.GetMultiBox()->GetBlocks().Num() > 1)
	{
		MenuBuilder.AddMenuSeparator();
	}

	FClassViewerInitializationOptions InitOptions;
	InitOptions.Mode = EClassViewerMode::ClassPicker;
	InitOptions.bShowObjectRootClass = false;
	InitOptions.bShowUnloadedBlueprints = true;
	InitOptions.bShowNoneOption = false;
	InitOptions.bEnableClassDynamicLoading = true;
	InitOptions.bExpandRootNodes = true;
	InitOptions.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	InitOptions.ClassFilters.Add(MakeShared<FNotifyStateClassFilter>(MotionAnim));
	//InitOptions.ClassFilter = MakeShared<FNotifyStateClassFilter>(MotionAnim);
	InitOptions.bShowBackgroundBorder = false;

	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");
	MenuBuilder.AddWidget(
		SNew(SBox)
		.MinDesiredWidth(300.0f)
		.MaxDesiredHeight(400.0f)
		[
			ClassViewerModule.CreateClassViewer(InitOptions,
			FOnClassPicked::CreateLambda([this, bIsReplaceWithMenu](UClass* InClass)
				{
					FSlateApplication::Get().DismissAllMenus();
					if (bIsReplaceWithMenu)
					{
						ReplaceSelectedWithNotify(MakeBlueprintNotifyName(InClass->GetName()), InClass);
					}
					else
					{
						CreateNewNotifyAtCursor(MakeBlueprintNotifyName(InClass->GetName()), InClass);
					}
				}
			))
		],
		FText(), true, false);
}

void SMotionTagTrack::FillNewNotifyStateMenu(FMenuBuilder& MenuBuilder, bool bIsReplaceWithMenu /* = false */)
{
	//MakeNewNotifyPicker<UAnimNotifyState>(MenuBuilder, bIsReplaceWithMenu);
	MakeNewNotifyPicker<UTagSection>(MenuBuilder, bIsReplaceWithMenu);
}

void SMotionTagTrack::FillNewNotifyMenu(FMenuBuilder& MenuBuilder, bool bIsReplaceWithMenu /* = false */)
{
	// now add custom anim notifiers
	//USkeleton* SeqSkeleton = MotionAnim->AnimAsset->GetSkeleton();
	//if (SeqSkeleton)
	//{
	//	MenuBuilder.BeginSection("AnimNotifySkeletonSubMenu", LOCTEXT("NewNotifySubMenu_Skeleton", "Skeleton Notifies"));
	//	{
	//		if (!bIsReplaceWithMenu)
	//		{
	//			FUIAction UIAction;
	//			UIAction.ExecuteAction.BindSP(
	//				this, &SMotionTagTrack::OnNewNotifyClicked);
	//			MenuBuilder.AddMenuEntry(LOCTEXT("NewNotify", "New Notify..."), LOCTEXT("NewNotifyToolTip", "Create a new animation notify on the skeleton"), FSlateIcon(), UIAction);
	//		}

	//		MenuBuilder.AddSubMenu(
	//			LOCTEXT("NewTagSubMenu_Skeleton", "Skeleton Tags"),
	//			LOCTEXT("NewTagSubMenu_Skeleton_Tooltip", "Choose from custom tags on the skeleton"),
	//			FNewMenuDelegate::CreateLambda([this, SeqSkeleton, bIsReplaceWithMenu](FMenuBuilder& InSubMenuBuilder)
	//				{
	//					ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	//					TSharedRef<IEditableSkeleton> EditableSkeleton = SkeletonEditorModule.CreateEditableSkeleton(SeqSkeleton);

	//					InSubMenuBuilder.AddWidget
	//					(
	//						SNew(SBox)
	//						.MinDesiredWidth(300.0f)
	//						.MaxDesiredHeight(400.0f)
	//						/*[
	//							SNew(SSkeletonAnimNotifies, EditableSkeleton)
	//							.IsPicker(true)
	//							.OnItemSelected_Lambda([this, bIsReplaceWithMenu](const FName& InNotifyName)
	//							{
	//								FSlateApplication::Get().DismissAllMenus();

	//								if (!bIsReplaceWithMenu)
	//								{
	//									CreateNewNotifyAtCursor(InNotifyName.ToString(), nullptr);
	//								}
	//								else
	//								{
	//									ReplaceSelectedWithNotify(InNotifyName.ToString(), nullptr);
	//								}
	//							})
	//						],*/
	//						, FText(), true, false
	//					);
	//				}));
	//	}
	//	MenuBuilder.EndSection();
	//}

	// Add a notify picker
	MakeNewNotifyPicker<UTagPoint>(MenuBuilder, bIsReplaceWithMenu);
}

FAnimNotifyEvent& SMotionTagTrack::CreateNewBlueprintNotify(FString NewNotifyName, FString BlueprintPath, float StartTime)
{
	TSubclassOf<UObject> BlueprintClass = GetBlueprintClassFromPath(BlueprintPath);
	check(BlueprintClass);
	return CreateNewNotify(NewNotifyName, BlueprintClass, StartTime);
}

FAnimNotifyEvent& SMotionTagTrack::CreateNewNotify(FString NewNotifyName, UClass* NotifyClass, float StartTime)
{
	// Insert a new notify record and spawn the new notify object
	int32 NewNotifyIndex = MotionAnim->Tags.Add(FAnimNotifyEvent());
	FAnimNotifyEvent& NewEvent = MotionAnim->Tags[NewNotifyIndex];
	NewEvent.NotifyName = FName(*NewNotifyName);
	NewEvent.Guid = FGuid::NewGuid();

	UAnimSequenceBase* MotionSequence = Cast<UAnimSequenceBase>(MotionAnim->AnimAsset);

	NewEvent.Link(MotionSequence, StartTime);
	NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(MotionSequence->CalculateOffsetForNotify(StartTime));
	NewEvent.TrackIndex = TrackIndex;

	if (NotifyClass)
	{
		MotionAnim->Modify();
		
		UObject* AnimNotifyClass = NewObject<UObject>(MotionAnim, NotifyClass, NAME_None, RF_Transactional); //The outer object should probably be the MotionAnimData
		NewEvent.NotifyStateClass = Cast<UAnimNotifyState>(AnimNotifyClass);
		NewEvent.Notify = Cast<UAnimNotify>(AnimNotifyClass);

		// Set default duration to 1 frame for AnimNotifyState.
		if (NewEvent.NotifyStateClass)
		{
			NewEvent.SetDuration(1 / 30.f);
			NewEvent.EndLink.Link(Cast<UAnimSequenceBase>(MotionAnim->AnimAsset), NewEvent.EndLink.GetTime());
		}
	}
	else
	{
		NewEvent.Notify = NULL;
		NewEvent.NotifyStateClass = NULL;
	}

	if (NewEvent.Notify)
	{
		TArray<FAssetData> SelectedAssets;
		AssetSelectionUtils::GetSelectedAssets(SelectedAssets);

		for (TFieldIterator<FObjectProperty> PropIt(NewEvent.Notify->GetClass()); PropIt; ++PropIt)
		{
			if (PropIt->GetBoolMetaData(TEXT("ExposeOnSpawn")))
			{
				FObjectProperty* Property = *PropIt;
				const FAssetData* Asset = SelectedAssets.FindByPredicate([Property](const FAssetData& Other)
					{
						return Other.GetAsset()->IsA(Property->PropertyClass);
					});

				if (Asset)
				{
					uint8* Offset = (*PropIt)->ContainerPtrToValuePtr<uint8>(NewEvent.Notify);
					(*PropIt)->ImportText_Direct(*Asset->GetAsset()->GetPathName(), Offset, NewEvent.Notify, 0);
					break;
				}
			}
		}

		NewEvent.Notify->OnAnimNotifyCreatedInEditor(NewEvent);
	}
	else if (NewEvent.NotifyStateClass)
	{
		NewEvent.NotifyStateClass->OnAnimNotifyCreatedInEditor(NewEvent);
	}

	return NewEvent;
}

void SMotionTagTrack::CreateNewBlueprintNotifyAtCursor(FString NewNotifyName, FString BlueprintPath)
{
	TSubclassOf<UObject> BlueprintClass = GetBlueprintClassFromPath(BlueprintPath);
	check(BlueprintClass);
	CreateNewNotifyAtCursor(NewNotifyName, BlueprintClass);
}

void SMotionTagTrack::CreateNewNotifyAtCursor(FString NewNotifyName, UClass* NotifyClass)
{
	const FScopedTransaction Transaction(LOCTEXT("AddTagEvent", "Add Motion Tag"));
	MotionAnim->Modify();
	CreateNewNotify(NewNotifyName, NotifyClass, LastClickedTime);
	OnUpdatePanel.ExecuteIfBound();
}

void SMotionTagTrack::ReplaceSelectedWithBlueprintNotify(FString NewNotifyName, FString BlueprintPath)
{
	OnReplaceSelectedWithBlueprintTag.ExecuteIfBound(NewNotifyName, BlueprintPath);
}

void SMotionTagTrack::ReplaceSelectedWithNotify(FString NewNotifyName, UClass* NotifyClass)
{
	OnReplaceSelectedWithTag.ExecuteIfBound(NewNotifyName, NotifyClass);
}

bool SMotionTagTrack::IsValidToPlace(UClass* NotifyClass) const
{
	if (NotifyClass && NotifyClass->IsChildOf(UAnimNotify::StaticClass()))
	{
		UAnimNotify* DefaultNotify = NotifyClass->GetDefaultObject<UAnimNotify>();
		return DefaultNotify->CanBePlaced(Cast<UAnimSequenceBase>(MotionAnim->AnimAsset));
	}

	if (NotifyClass && NotifyClass->IsChildOf(UAnimNotifyState::StaticClass()))
	{
		UAnimNotifyState* DefaultNotifyState = NotifyClass->GetDefaultObject<UAnimNotifyState>();
		return DefaultNotifyState->CanBePlaced(Cast<UAnimSequenceBase>(MotionAnim->AnimAsset));
	}

	return true;
}

TSubclassOf<UObject> SMotionTagTrack::GetBlueprintClassFromPath(FString BlueprintPath)
{
	TSubclassOf<UObject> BlueprintClass = NULL;
	if (!BlueprintPath.IsEmpty())
	{
		UBlueprint* BlueprintLibPtr = LoadObject<UBlueprint>(NULL, *BlueprintPath, NULL, 0, NULL);
		BlueprintClass = BlueprintLibPtr->GeneratedClass;
	}
	return BlueprintClass;
}

FReply SMotionTagTrack::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bool bLeftMouseButton = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	bool bRightMouseButton = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
	bool bShift = MouseEvent.IsShiftDown();
	bool bCtrl = MouseEvent.IsControlDown();

	if (bRightMouseButton)
	{
		TSharedPtr<SWidget> WidgetToFocus;
		WidgetToFocus = SummonContextMenu(MyGeometry, MouseEvent);

		return (WidgetToFocus.IsValid())
			? FReply::Handled().ReleaseMouseCapture().SetUserFocus(WidgetToFocus.ToSharedRef(), EFocusCause::SetDirectly)
			: FReply::Handled().ReleaseMouseCapture();
	}
	else if (bLeftMouseButton)
	{
		FVector2D CursorPos = MouseEvent.GetScreenSpacePosition();
		CursorPos = MyGeometry.AbsoluteToLocal(CursorPos);
		int32 NotifyIndex = GetHitNotifyNode(MyGeometry, CursorPos);
		LastClickedTime = CalculateTime(MyGeometry, MouseEvent.GetScreenSpacePosition());

		if (NotifyIndex == INDEX_NONE)
		{
			// Clicked in empty space, clear selection
			OnDeselectAllTags.ExecuteIfBound();
		}
		else
		{
			if (bCtrl)
			{
				ToggleTrackObjectNodeSelectionStatus(NotifyIndex);
			}
			else
			{
				SelectTrackObjectNode(NotifyIndex, bShift);
			}
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SMotionTagTrack::SelectTrackObjectNode(int32 TrackNodeIndex, bool Append, bool bUpdateSelection)
{
	if (TrackNodeIndex != INDEX_NONE)
	{
		// Deselect all other notifies if necessary.
		if (MotionAnim && !Append)
		{
			OnDeselectAllTags.ExecuteIfBound();
		}

		// Check to see if we've already selected this node
		if (!SelectedNodeIndices.Contains(TrackNodeIndex))
		{
			// select new one
			if (NotifyNodes.IsValidIndex(TrackNodeIndex))
			{
				TSharedPtr<SMotionTagNode> Node = NotifyNodes[TrackNodeIndex];
				Node->bSelected = true;
				SelectedNodeIndices.Add(TrackNodeIndex);

				if (bUpdateSelection)
				{
					SendSelectionChanged();
				}
			}
		}
	}
}

void SMotionTagTrack::ToggleTrackObjectNodeSelectionStatus(int32 TrackNodeIndex, bool bUpdateSelection)
{
	check(NotifyNodes.IsValidIndex(TrackNodeIndex));

	bool bSelected = SelectedNodeIndices.Contains(TrackNodeIndex);
	if (bSelected)
	{
		SelectedNodeIndices.Remove(TrackNodeIndex);
	}
	else
	{
		SelectedNodeIndices.Add(TrackNodeIndex);
	}

	TSharedPtr<SMotionTagNode> Node = NotifyNodes[TrackNodeIndex];
	Node->bSelected = !Node->bSelected;

	if (bUpdateSelection)
	{
		SendSelectionChanged();
	}
}

void SMotionTagTrack::DeselectTrackObjectNode(int32 TrackNodeIndex, bool bUpdateSelection)
{
	check(NotifyNodes.IsValidIndex(TrackNodeIndex));
	TSharedPtr<SMotionTagNode> Node = NotifyNodes[TrackNodeIndex];
	Node->bSelected = false;

	int32 ItemsRemoved = SelectedNodeIndices.Remove(TrackNodeIndex);
	check(ItemsRemoved > 0);

	if (bUpdateSelection)
	{
		SendSelectionChanged();
	}
}

void SMotionTagTrack::DeselectAllNotifyNodes(bool bUpdateSelectionSet)
{
	for (TSharedPtr<SMotionTagNode> Node : NotifyNodes)
	{
		Node->bSelected = false;
	}
	SelectedNodeIndices.Empty();

	if (bUpdateSelectionSet)
	{
		SendSelectionChanged();
	}
}

void SMotionTagTrack::SelectNodesByGuid(const TSet<FGuid>& InGuids, bool bUpdateSelectionSet)
{
	SelectedNodeIndices.Empty();

	for (int32 NodeIndex = 0; NodeIndex < NotifyNodes.Num(); ++NodeIndex)
	{
		TSharedPtr<SMotionTagNode> Node = NotifyNodes[NodeIndex];
		Node->bSelected = InGuids.Contains(Node->NodeObjectInterface->GetGuid());
		if (Node->bSelected)
		{
			SelectedNodeIndices.Add(NodeIndex);
		}
	}

	if (bUpdateSelectionSet)
	{
		SendSelectionChanged();
	}
}

TSharedPtr<SWidget> SMotionTagTrack::SummonContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FVector2D CursorPos = MouseEvent.GetScreenSpacePosition();
	int32 NodeIndex = GetHitNotifyNode(MyGeometry, MyGeometry.AbsoluteToLocal(CursorPos));
	LastClickedTime = CalculateTime(MyGeometry, MouseEvent.GetScreenSpacePosition());

	const bool bCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bCloseWindowAfterMenuSelection, WeakCommandList.Pin());
	FUIAction NewAction;

	INodeObjectInterface* NodeObject = NodeIndex != INDEX_NONE ? NotifyNodes[NodeIndex]->NodeObjectInterface : nullptr;
	FAnimNotifyEvent* NotifyEvent = NodeObject ? NodeObject->GetNotifyEvent() : nullptr;
	int32 NotifyIndex = NotifyEvent ? AnimNotifies.IndexOfByKey(NotifyEvent) : INDEX_NONE;

	UAnimSequenceBase* MotionSequence = Cast<UAnimSequenceBase>(MotionAnim->AnimAsset);
	int32 NumSampledKeys = MotionSequence->GetNumberOfSampledKeys();
	
	MenuBuilder.BeginSection("AnimTag", LOCTEXT("TagHeading", "Tag"));
	{
		if (NodeObject)
		{
			if (!NotifyNodes[NodeIndex]->bSelected)
			{
				SelectTrackObjectNode(NodeIndex, MouseEvent.IsControlDown());
			}

			if (IsSingleNodeSelected())
			{
				// Add item to directly set notify time
				TSharedRef<SWidget> TimeWidget =
					SNew(SBox)
					.HAlign(HAlign_Right)
					.ToolTipText(LOCTEXT("SetTimeToolTip", "Set the time of this tag directly"))
					[
						SNew(SBox)
						.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
					.WidthOverride(100.0f)
					[
						SNew(SNumericEntryBox<float>)
						.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
					.MinValue(0.0f)
					.MaxValue(MotionAnim->GetPlayLength())
					.Value(NodeObject->GetTime())
					.AllowSpin(false)
					.OnValueCommitted_Lambda([this, NodeIndex](float InValue, ETextCommit::Type InCommitType)
						{
							if (InCommitType == ETextCommit::OnEnter && NotifyNodes.IsValidIndex(NodeIndex))
							{
								INodeObjectInterface* LocalNodeObject = NotifyNodes[NodeIndex]->NodeObjectInterface;

								float NewTime = FMath::Clamp(InValue, 0.0f, (float)MotionAnim->GetPlayLength() - LocalNodeObject->GetDuration());
								LocalNodeObject->SetTime(NewTime);

								if (FAnimNotifyEvent* Event = LocalNodeObject->GetNotifyEvent())
								{
									UAnimSequenceBase* ThisSequence = Cast<UAnimSequenceBase>(MotionAnim->AnimAsset);

										Event->RefreshTriggerOffset(ThisSequence->CalculateOffsetForNotify(Event->GetTime()));
										if (Event->GetDuration() > 0.0f)
										{
											Event->RefreshEndTriggerOffset(ThisSequence->CalculateOffsetForNotify(Event->GetTime() + Event->GetDuration()));
										}
									
								}
								OnUpdatePanel.ExecuteIfBound();

								FSlateApplication::Get().DismissAllMenus();
							}
						})
					]
					];

				MenuBuilder.AddWidget(TimeWidget, LOCTEXT("TimeMenuText", "Tag Begin Time"));

				// Add item to directly set notify frame
				TSharedRef<SWidget> FrameWidget =
					SNew(SBox)
					.HAlign(HAlign_Right)
					.ToolTipText(LOCTEXT("SetFrameToolTip", "Set the frame of this tag directly"))
					[
						SNew(SBox)
						.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
					.WidthOverride(100.0f)
					[
						SNew(SNumericEntryBox<int32>)
						.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
					.MinValue(0)
					.MaxValue(NumSampledKeys)
					.Value(MotionSequence->GetFrameAtTime(NodeObject->GetTime()))
					.AllowSpin(false)
					.OnValueCommitted_Lambda([this, NodeIndex](int32 InValue, ETextCommit::Type InCommitType)
						{
							if (InCommitType == ETextCommit::OnEnter && NotifyNodes.IsValidIndex(NodeIndex))
							{
								INodeObjectInterface* LocalNodeObject = NotifyNodes[NodeIndex]->NodeObjectInterface;

								UAnimSequenceBase* ThisSequence = Cast<UAnimSequenceBase>(MotionAnim->AnimAsset);

								float NewTime = FMath::Clamp(ThisSequence->GetTimeAtFrame(InValue), 0.0f, (float)MotionAnim->GetPlayLength() - LocalNodeObject->GetDuration());
								LocalNodeObject->SetTime(NewTime);

								if (FAnimNotifyEvent* Event = LocalNodeObject->GetNotifyEvent())
								{
									Event->RefreshTriggerOffset(ThisSequence->CalculateOffsetForNotify(Event->GetTime()));
									if (Event->GetDuration() > 0.0f)
									{
										Event->RefreshEndTriggerOffset(ThisSequence->CalculateOffsetForNotify(Event->GetTime() + Event->GetDuration()));
									}
								}
								OnUpdatePanel.ExecuteIfBound();

								FSlateApplication::Get().DismissAllMenus();
							}
						})
					]
					];

				MenuBuilder.AddWidget(FrameWidget, LOCTEXT("FrameMenuText", "Tag Frame"));

				if (NotifyEvent)
				{
					// add menu to get threshold weight for triggering this notify
					TSharedRef<SWidget> ThresholdWeightWidget =
						SNew(SBox)
						.HAlign(HAlign_Right)
						.ToolTipText(LOCTEXT("MinTriggerWeightToolTip", "The minimum weight to trigger this tag"))
						[
							SNew(SBox)
							.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
						.WidthOverride(100.0f)
						[
							SNew(SNumericEntryBox<float>)
							.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
						.MinValue(0.0f)
						.MaxValue(1.0f)
						.Value(NotifyEvent->TriggerWeightThreshold)
						.AllowSpin(false)
						.OnValueCommitted_Lambda([this, NotifyIndex](float InValue, ETextCommit::Type InCommitType)
							{
								if (InCommitType == ETextCommit::OnEnter && AnimNotifies.IsValidIndex(NotifyIndex))
								{
									float NewWeight = FMath::Max(InValue, ZERO_ANIMWEIGHT_THRESH);
									AnimNotifies[NotifyIndex]->TriggerWeightThreshold = NewWeight;

									FSlateApplication::Get().DismissAllMenus();
								}
							})
						]
						];

					MenuBuilder.AddWidget(ThresholdWeightWidget, LOCTEXT("MinTriggerWeight", "Min Trigger Weight"));

					// Add menu for changing duration if this is an AnimNotifyState
					if (NotifyEvent->NotifyStateClass)
					{
						TSharedRef<SWidget> NotifyStateDurationWidget =
							SNew(SBox)
							.HAlign(HAlign_Right)
							.ToolTipText(LOCTEXT("SetMotionStateDuration_ToolTip", "The duration of this Motion Tag Section"))
							[
								SNew(SBox)
								.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
							.WidthOverride(100.0f)
							[
								SNew(SNumericEntryBox<float>)
								.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
							.MinValue(SMotionTagNode::MinimumStateDuration)
							.MinSliderValue(SMotionTagNode::MinimumStateDuration)
							.MaxSliderValue(100.0f)
							.Value(NotifyEvent->GetDuration())
							.AllowSpin(false)
							.OnValueCommitted_Lambda([this, NotifyIndex](float InValue, ETextCommit::Type InCommitType)
								{
									if (InCommitType == ETextCommit::OnEnter && AnimNotifies.IsValidIndex(NotifyIndex))
									{
										float NewDuration = FMath::Max(InValue, SMotionTagNode::MinimumStateDuration);
										float MaxDuration = MotionAnim->GetPlayLength() - AnimNotifies[NotifyIndex]->GetTime();
										NewDuration = FMath::Min(NewDuration, MaxDuration);
										AnimNotifies[NotifyIndex]->SetDuration(NewDuration);

										// If we have a delegate bound to refresh the offsets, call it.
										// This is used by the montage editor to keep the offsets up to date.
										OnRequestRefreshOffsets.ExecuteIfBound();

										FSlateApplication::Get().DismissAllMenus();
									}
								})
							]
							];

						MenuBuilder.AddWidget(NotifyStateDurationWidget, LOCTEXT("SetMotionSateDuration", "Motion Tag Section Duration"));

						TSharedRef<SWidget> NotifyStateDurationFramesWidget =
							SNew(SBox)
							.HAlign(HAlign_Right)
							.ToolTipText(LOCTEXT("SetMotionStateDurationFrames_ToolTip", "The duration of this Motion Tag Section in frames"))
							[
								SNew(SBox)
								.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
							.WidthOverride(100.0f)
							[
								SNew(SNumericEntryBox<int32>)
								.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
							.MinValue(1)
							.MinSliderValue(1)
							.MaxSliderValue(NumSampledKeys)
							.Value(MotionSequence->GetFrameAtTime(NotifyEvent->GetDuration()))
							.AllowSpin(false)
							.OnValueCommitted_Lambda([this, NotifyIndex](int32 InValue, ETextCommit::Type InCommitType)
								{
									if (InCommitType == ETextCommit::OnEnter && AnimNotifies.IsValidIndex(NotifyIndex))
									{
										UAnimSequenceBase* ThisSequence = Cast<UAnimSequenceBase>(MotionAnim->AnimAsset);

										float NewDuration = FMath::Max(ThisSequence->GetTimeAtFrame(InValue), SMotionTagNode::MinimumStateDuration);
										float MaxDuration = MotionAnim->GetPlayLength() - AnimNotifies[NotifyIndex]->GetTime();
										NewDuration = FMath::Min(NewDuration, MaxDuration);
										AnimNotifies[NotifyIndex]->SetDuration(NewDuration);

										// If we have a delegate bound to refresh the offsets, call it.
										// This is used by the montage editor to keep the offsets up to date.
										OnRequestRefreshOffsets.ExecuteIfBound();

										FSlateApplication::Get().DismissAllMenus();
									}
								})
							]
							];

						MenuBuilder.AddWidget(NotifyStateDurationFramesWidget, LOCTEXT("SetMotionStateDurationFrames", "Motion Tag Section Frames"));
					}
				}
			}
		}
		else
		{
			MenuBuilder.AddSubMenu(
				NSLOCTEXT("NewTagSubMenu", "NewTagSubMenuAddTag", "Add Tag..."),
				NSLOCTEXT("NewTagSubMenu", "NewTagSubMenuAddTagToolTip", "Add AnimTagEvent"),
				FNewMenuDelegate::CreateRaw(this, &SMotionTagTrack::FillNewNotifyMenu, false));

			MenuBuilder.AddSubMenu(
				NSLOCTEXT("NewTagSubMenu", "NewTagSubMenuAddNTagSection", "Add Tag Section..."),
				NSLOCTEXT("NewTagSubMenu", "NewTagSubMenuAddTagSectionToolTip", "Add AnimTagSection"),
				FNewMenuDelegate::CreateRaw(this, &SMotionTagTrack::FillNewNotifyStateMenu, false));

			/*MenuBuilder.AddMenuEntry(
				NSLOCTEXT("NewTagSubMenu", "ManageNotifies", "Manage Notifies..."),
				NSLOCTEXT("NewTagSubMenu", "ManageNotifiesToolTip", "Opens the Manage Notifies window"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SMotionTagTrack::OnManageNotifies)));*/
		}
	}
	MenuBuilder.EndSection(); //AnimNotify

	NewAction.CanExecuteAction = 0;

	MenuBuilder.BeginSection("AnimEdit", LOCTEXT("TagEditHeading", "Edit"));
	{
		if (NodeObject)
		{
			// copy notify menu item
			MenuBuilder.AddMenuEntry(FMotionTagPanelCommands::Get().CopyTags);

			// allow it to delete
			MenuBuilder.AddMenuEntry(FMotionTagPanelCommands::Get().DeleteTag);

			if (NotifyEvent)
			{
				// For the "Replace With..." menu, make sure the current AnimNotify selection is valid for replacement
				if (OnGetIsMotionTagSelectionValidForReplacement.IsBound() && OnGetIsMotionTagSelectionValidForReplacement.Execute())
				{
					// If this is an AnimNotifyState (has duration) allow it to be replaced with other AnimNotifyStates
					if (NotifyEvent->NotifyStateClass)
					{
						MenuBuilder.AddSubMenu(
							NSLOCTEXT("NewTagSubMenu", "NewTagSubMenuReplaceWithTagSection", "Replace with Tag Section..."),
							NSLOCTEXT("NewTagSubMenu", "NewTagSubMenuReplaceWithTagSectionToolTip", "Replace with MotionTagSection"),
							FNewMenuDelegate::CreateRaw(this, &SMotionTagTrack::FillNewNotifyStateMenu, true));
					}
					// If this is a regular AnimNotify (no duration) allow it to be replaced with other AnimNotifies
					else
					{
						MenuBuilder.AddSubMenu(
							NSLOCTEXT("NewTagSubMenu", "NewTagSubMenuReplaceWithTag", "Replace with Tag..."),
							NSLOCTEXT("NewTagSubMenu", "NewTagSubMenuReplaceWithTagToolTip", "Replace with AnimTagEvent"),
							FNewMenuDelegate::CreateRaw(this, &SMotionTagTrack::FillNewNotifyMenu, true));
					}
				}
			}
		}
		else
		{
			FString PropertyString;
			const TCHAR* Buffer;
			float OriginalTime;
			float OriginalLength;
			int32 TrackSpan;

			//Check whether can we show menu item to paste anim notify event
			if (ReadNotifyPasteHeader(PropertyString, Buffer, OriginalTime, OriginalLength, TrackSpan))
			{
				// paste notify menu item
				if (IsSingleNodeInClipboard())
				{
					MenuBuilder.AddMenuEntry(FMotionTagPanelCommands::Get().PasteTags);
				}
				else
				{
					NewAction.ExecuteAction.BindRaw(
						this, &SMotionTagTrack::OnPasteNotifyClicked, ETagPasteMode::MousePosition, ETagPasteMultipleMode::Relative);

					MenuBuilder.AddMenuEntry(LOCTEXT("PasteMultRel", "Paste Multiple Relative"), LOCTEXT("PasteMultRelToolTip", "Paste multiple Tags beginning at the mouse cursor, maintaining the same relative spacing as the source."), FSlateIcon(), NewAction);

					MenuBuilder.AddMenuEntry(FMotionTagPanelCommands::Get().PasteTags, NAME_None, LOCTEXT("PasteMultAbs", "Paste Multiple Absolute"), LOCTEXT("PasteMultAbsToolTip", "Paste multiple tags beginning at the mouse cursor, maintaining absolute spacing."));
				}

				if (OriginalTime < MotionAnim->GetPlayLength())
				{
					NewAction.ExecuteAction.BindRaw(
						this, &SMotionTagTrack::OnPasteNotifyClicked, ETagPasteMode::OriginalTime, ETagPasteMultipleMode::Absolute);

					FText DisplayText = FText::Format(LOCTEXT("PasteAtOriginalTime", "Paste at original time ({0})"), FText::AsNumber(OriginalTime));
					MenuBuilder.AddMenuEntry(DisplayText, LOCTEXT("PasteAtOriginalTimeToolTip", "Paste animation tag event at the time it was set to when it was copied"), FSlateIcon(), NewAction);
				}

			}
		}
	}
	MenuBuilder.EndSection(); //AnimEdit

	if (NotifyEvent)
	{
		UObject* NotifyObject = NotifyEvent->Notify;
		NotifyObject = NotifyObject ? NotifyObject : NotifyEvent->NotifyStateClass;

		MenuBuilder.BeginSection("ViewSource", LOCTEXT("TagViewHeading", "View"));

		if (NotifyObject)
		{
			if (Cast<UBlueprintGeneratedClass>(NotifyObject->GetClass()))
			{
				if (UBlueprint* Blueprint = Cast<UBlueprint>(NotifyObject->GetClass()->ClassGeneratedBy))
				{
					NewAction.ExecuteAction.BindRaw(
						this, &SMotionTagTrack::OnOpenNotifySource, Blueprint);
					MenuBuilder.AddMenuEntry(LOCTEXT("OpenTagBlueprint", "Open Tag Blueprint"), LOCTEXT("OpenNotifyBlueprintTooltip", "Opens the source blueprint for this Tag"), FSlateIcon(), NewAction);
				}
			}
		}
		else
		{
			// skeleton notify
			NewAction.ExecuteAction.BindRaw(
				this, &SMotionTagTrack::OnFilterSkeletonNotify, NotifyEvent->NotifyName);
			MenuBuilder.AddMenuEntry(LOCTEXT("FindTagReferences", "Find References"), LOCTEXT("FindTagReferencesTooltip", "Find all references to this skeleton Tag in the asset browser"), FSlateIcon(), NewAction);
		}

		MenuBuilder.EndSection(); //ViewSource
	}

	FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();

	// Display the newly built menu
	FSlateApplication::Get().PushMenu(SharedThis(this), WidgetPath, MenuBuilder.MakeWidget(), CursorPos, FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

	return TSharedPtr<SWidget>();
}

bool SMotionTagTrack::CanPasteAnimNotify() const
{
	FString PropertyString;
	const TCHAR* Buffer;
	float OriginalTime;
	float OriginalLength;
	int32 TrackSpan;
	return ReadNotifyPasteHeader(PropertyString, Buffer, OriginalTime, OriginalLength, TrackSpan);
}

void SMotionTagTrack::OnPasteNotifyClicked(ETagPasteMode::Type PasteMode, ETagPasteMultipleMode::Type MultiplePasteType)
{
	float ClickTime = PasteMode == ETagPasteMode::MousePosition ? LastClickedTime : -1.0f;
	OnPasteNodes.ExecuteIfBound(this, ClickTime, PasteMode, MultiplePasteType);
}

void SMotionTagTrack::OnManageNotifies()
{
	//OnInvokeTab.ExecuteIfBound(FPersonaTabs::SkeletonAnimNotifiesID);
}

void SMotionTagTrack::OnOpenNotifySource(UBlueprint* InSourceBlueprint) const
{
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(InSourceBlueprint);
}

void SMotionTagTrack::OnFilterSkeletonNotify(FName InName)
{
	// Open asset browser first
	//OnInvokeTab.ExecuteIfBound(FPersonaTabs::AssetBrowserID);

	//IAssetEditorInstance* AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Sequence, true);
	//check(AssetEditor->GetEditorName() == TEXT("AnimationEditor"));
	//IAnimationEditor* AnimationEditor = static_cast<IAnimationEditor*>(AssetEditor);
	//AnimationEditor->GetAssetBrowser()->FilterBySkeletonNotify(InName);
}

bool SMotionTagTrack::IsSingleNodeSelected()
{
	return SelectedNodeIndices.Num() == 1;
}

bool SMotionTagTrack::IsSingleNodeInClipboard()
{
	FString PropString;
	const TCHAR* Buffer;
	float OriginalTime;
	float OriginalLength;
	int32 TrackSpan;
	uint32 Count = 0;
	if (ReadNotifyPasteHeader(PropString, Buffer, OriginalTime, OriginalLength, TrackSpan))
	{
		// If reading a single line empties the buffer then we only have one notify in there.
		FString TempLine;
		FParse::Line(&Buffer, TempLine);
		return *Buffer == 0;
	}
	return false;
}

void SMotionTagTrack::OnNewNotifyClicked()
{
	// Show dialog to enter new track name
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label(LOCTEXT("NewTagLabel", "Tag Name"))
		.OnTextCommitted(this, &SMotionTagTrack::AddNewNotify);

	// Show dialog to enter new event name
	FSlateApplication::Get().PushMenu(
		AsShared(), // Menu being summoned from a menu that is closing: Parent widget should be k2 not the menu thats open or it will be closed when the menu is dismissed
		FWidgetPath(),
		TextEntry,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
	);
}

void SMotionTagTrack::AddNewNotify(const FText& NewNotifyName, ETextCommit::Type CommitInfo)
{
	USkeleton* SeqSkeleton = MotionAnim->AnimAsset->GetSkeleton();
	if ((CommitInfo == ETextCommit::OnEnter) && SeqSkeleton)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddNewTagEvent", "Add New Motion Tag"));
		FName NewName = FName(*NewNotifyName.ToString());

		ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
		TSharedRef<IEditableSkeleton> EditableSkeleton = SkeletonEditorModule.CreateEditableSkeleton(SeqSkeleton);

		EditableSkeleton->AddNotify(NewName);

		FBlueprintActionDatabase::Get().RefreshAssetActions(SeqSkeleton);

		CreateNewNotifyAtCursor(NewNotifyName.ToString(), (UClass*)nullptr);
	}

	FSlateApplication::Get().DismissAllMenus();
}

void SMotionTagTrack::Update()
{
	NotifyPairs.Empty();
	NotifyNodes.Empty();

	TrackArea->SetContent(
		SAssignNew(NodeSlots, SOverlay)
	);

	if (AnimNotifies.Num() > 0)
	{
		TArray<TSharedPtr<FTimingRelevantElementBase>> TimingElements;
		SMotionTimingPanel::GetTimingRelevantElements(Cast<UAnimSequenceBase>(MotionAnim->AnimAsset), TimingElements);
		for (int32 NotifyIndex = 0; NotifyIndex < AnimNotifies.Num(); ++NotifyIndex)
		{
			TSharedPtr<FTimingRelevantElementBase> Element;
			FAnimNotifyEvent* Event = AnimNotifies[NotifyIndex];

			for (int32 Idx = 0; Idx < TimingElements.Num(); ++Idx)
			{
				Element = TimingElements[Idx];

				if (Element->GetType() == ETimingElementType::NotifyStateBegin
					|| Element->GetType() == ETimingElementType::BranchPointNotify
					|| Element->GetType() == ETimingElementType::QueuedNotify)
				{
					// Only the notify type will return the type flags above
					FTimingRelevantElement_Notify* NotifyElement = static_cast<FTimingRelevantElement_Notify*>(Element.Get());
					if (Event == &MotionAnim->Tags[NotifyElement->NotifyIndex])
					{
						break;
					}
				}
			}

			TSharedPtr<SMotionTagNode> AnimNotifyNode = nullptr;
			TSharedPtr<SMotionTagPair> NotifyPair = nullptr;
			//TSharedPtr<SMotionTimingNode> TimingNode = nullptr;
			//TSharedPtr<SMotionTimingNode> EndTimingNode = nullptr;

			// Create visibility attribute to control timing node visibility for notifies
			TAttribute<EVisibility> TimingNodeVisibility = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(
				[this]()
				{
					/*if (OnGetTimingNodeVisibility.IsBound())
					{
						return OnGetTimingNodeVisibility.Execute(ETimingElementType::QueuedNotify);
					}*/
					return EVisibility(EVisibility::Hidden);
				}));

			/*SAssignNew(TimingNode, SMotionTimingNode)
				.InElement(Element)
				.bUseTooltip(true)
				.Visibility(TimingNodeVisibility);*/

			if (Event->NotifyStateClass)
			{
				TSharedPtr<FTimingRelevantElementBase>* FoundStateEndElement = TimingElements.FindByPredicate([Event](TSharedPtr<FTimingRelevantElementBase>& ElementToTest)
					{
						if (ElementToTest.IsValid() && ElementToTest->GetType() == ETimingElementType::NotifyStateEnd)
						{
							FTimingRelevantElement_NotifyStateEnd* StateElement = static_cast<FTimingRelevantElement_NotifyStateEnd*>(ElementToTest.Get());
							return &(StateElement->Sequence->Notifies[StateElement->NotifyIndex]) == Event;
						}
						return false;
					});

				//if (FoundStateEndElement)
				//{
				//	// Create an end timing node if we have a state
				//	SAssignNew(EndTimingNode, SMotionTimingNode)
				//		.InElement(*FoundStateEndElement)
				//		.bUseTooltip(true)
				//		.Visibility(TimingNodeVisibility);
				//}
			}

			SAssignNew(AnimNotifyNode, SMotionTagNode)
				.MotionAnim(MotionAnim)
				.AnimNotify(Event)
				.OnNodeDragStarted(this, &SMotionTagTrack::OnTagNodeDragStarted, NotifyIndex)
				.OnUpdatePanel(OnUpdatePanel)
				.PanTrackRequest(OnRequestTrackPan)
				.ViewInputMin(ViewInputMin)
				.ViewInputMax(ViewInputMax)
				.OnSnapPosition(OnSnapPosition)
				.OnSelectionChanged(OnSelectionChanged);
			//	.StateEndTimingNode(EndTimingNode);

			SAssignNew(NotifyPair, SMotionTagPair)
				.LeftContent()
				[
					SNew(SBox)
					//TimingNode.ToSharedRef()
				]
			.Node(AnimNotifyNode);

			NodeSlots->AddSlot()
				.Padding(TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SMotionTagTrack::GetNotifyTrackPadding, NotifyIndex)))
				[
					NotifyPair->AsShared()
				];

			NotifyNodes.Add(AnimNotifyNode);
			NotifyPairs.Add(NotifyPair);
		}
	}
}

int32 SMotionTagTrack::GetHitNotifyNode(const FGeometry& MyGeometry, const FVector2D& CursorPosition)
{
	for (int32 I = NotifyNodes.Num() - 1; I >= 0; --I) //Run through from 'top most' Notify to bottom
	{
		if (NotifyNodes[I].Get()->HitTest(MyGeometry, CursorPosition))
		{
			return I;
		}
	}

	return INDEX_NONE;
}

FReply SMotionTagTrack::OnTagNodeDragStarted(TSharedRef<SMotionTagNode> NotifyNode, const FPointerEvent& MouseEvent, const FVector2D& ScreenNodePosition, const bool bDragOnMarker, int32 NotifyIndex)
{
	// Check to see if we've already selected the triggering node
	if (!NotifyNode->bSelected)
	{
		SelectTrackObjectNode(NotifyIndex, MouseEvent.IsShiftDown(), false);
	}

	// Sort our nodes so we're acessing them in time order
	SelectedNodeIndices.Sort([this](const int32& A, const int32& B)
		{
			float TimeA = NotifyNodes[A]->NodeObjectInterface->GetTime();
			float TimeB = NotifyNodes[B]->NodeObjectInterface->GetTime();
			return TimeA < TimeB;
		});

	// If we're dragging one of the direction markers we don't need to call any further as we don't want the drag drop op
	if (!bDragOnMarker)
	{
		TArray<TSharedPtr<SMotionTagNode>> NodesToDrag;
		const float FirstNodeX = NotifyNodes[SelectedNodeIndices[0]]->GetWidgetPosition().X;

		TSharedRef<SOverlay> DragBox = SNew(SOverlay);
		for (auto Iter = SelectedNodeIndices.CreateIterator(); Iter; ++Iter)
		{
			TSharedPtr<SMotionTagNode> Node = NotifyNodes[*Iter];
			NodesToDrag.Add(Node);
		}

		FVector2D DecoratorPosition = NodesToDrag[0]->GetWidgetPosition();
		DecoratorPosition = CachedGeometry.LocalToAbsolute(DecoratorPosition);
		return OnNodeDragStarted.Execute(NodesToDrag, DragBox, MouseEvent.GetScreenSpacePosition(), DecoratorPosition, bDragOnMarker);
	}
	else
	{
		// Capture the mouse in the node
		return FReply::Handled().CaptureMouse(NotifyNode).UseHighPrecisionMouseMovement(NotifyNode);
	}
}

FReply SMotionTagTrack::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FVector2D CursorPos = MouseEvent.GetScreenSpacePosition();
	CursorPos = MyGeometry.AbsoluteToLocal(CursorPos);
	int32 HitIndex = GetHitNotifyNode(MyGeometry, CursorPos);

	if (HitIndex != INDEX_NONE)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			// Hit a node, record the mouse position for use later so we can know when / where a
			// drag happened on the node handles if necessary.
			NotifyNodes[HitIndex]->SetLastMouseDownPosition(CursorPos);

			return FReply::Handled().DetectDrag(NotifyNodes[HitIndex].ToSharedRef(), EKeys::LeftMouseButton);
		}
		else if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			// Hit a node, return handled so we can pop a context menu on mouse up
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

float SMotionTagTrack::CalculateTime(const FGeometry& MyGeometry, FVector2D NodePos, bool bInputIsAbsolute)
{
	if (bInputIsAbsolute)
	{
		NodePos = MyGeometry.AbsoluteToLocal(NodePos);
	}
	FTrackScaleInfo ScaleInfo(ViewInputMin.Get(), ViewInputMax.Get(), 0, 0, MyGeometry.Size);
	return FMath::Clamp<float>(ScaleInfo.LocalXToInput(NodePos.X), 0.f, MotionAnim->GetPlayLength());
}

FReply SMotionTagTrack::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	return FReply::Unhandled();
}

void SMotionTagTrack::HandleNodeDrop(TSharedPtr<SMotionTagNode> Node, float Offset)
{
	ensure(Node.IsValid());

	float LocalX = GetCachedGeometry().AbsoluteToLocal(Node->GetScreenPosition() + Offset).X;
	float SnapTime = Node->GetLastSnappedTime();
	float Time = SnapTime != -1.0f ? SnapTime : GetCachedScaleInfo().LocalXToInput(LocalX);
	Node->NodeObjectInterface->HandleDrop(MotionAnim, Time, TrackIndex);
}

void SMotionTagTrack::DisconnectSelectedNodesForDrag(TArray<TSharedPtr<SMotionTagNode>>& DragNodes)
{
	if (SelectedNodeIndices.Num() == 0)
	{
		return;
	}

	const float FirstNodeX = NotifyNodes[SelectedNodeIndices[0]]->GetWidgetPosition().X;

	for (auto Iter = SelectedNodeIndices.CreateIterator(); Iter; ++Iter)
	{
		TSharedPtr<SMotionTagNode> Node = NotifyNodes[*Iter];
		if (Node->NodeObjectInterface->GetNotifyEvent())
		{
			TSharedPtr<SMotionTagPair> Pair = NotifyPairs[*Iter];
			NodeSlots->RemoveSlot(Pair->AsShared());
		}
		else
		{
			NodeSlots->RemoveSlot(Node->AsShared());
		}

		DragNodes.Add(Node);
	}
}

void SMotionTagTrack::AppendSelectionToSet(FGraphPanelSelectionSet& SelectionSet)
{
	// Add our selection to the provided set
	for (int32 Index : SelectedNodeIndices)
	{
		if (FAnimNotifyEvent* Event = NotifyNodes[Index]->NodeObjectInterface->GetNotifyEvent())
		{
			if (Event->Notify)
			{
				SelectionSet.Add(Event->Notify);
			}
			else if (Event->NotifyStateClass)
			{
				SelectionSet.Add(Event->NotifyStateClass);
			}
		}
	}
}

void SMotionTagTrack::AppendSelectionToArray(TArray<INodeObjectInterface*>& Selection) const
{
	for (int32 Idx : SelectedNodeIndices)
	{
		Selection.Add(NotifyNodes[Idx]->NodeObjectInterface);
	}
}

void SMotionTagTrack::PasteSingleNotify(FString& NotifyString, float PasteTime)
{
	UAnimSequenceBase* MotionSequence = Cast<UAnimSequenceBase>(MotionAnim->AnimAsset);

	int32 NewIndex = MotionAnim->Tags.Add(FAnimNotifyEvent());
	FArrayProperty* ArrayProperty = NULL;
	uint8* PropertyData = MotionAnim->FindTagPropertyData(NewIndex, ArrayProperty);

	if (PropertyData && ArrayProperty)
	{
		ArrayProperty->Inner->ImportText_Direct(*NotifyString, PropertyData, NULL, PPF_Copy);

		FAnimNotifyEvent& NewNotify = MotionAnim->Tags[NewIndex];

		// We have to link to the montage / sequence again, we need a correct time set and we could be pasting to a new montage / sequence
		int32 NewSlotIndex = 0;
		float NewNotifyTime = PasteTime != 1.0f ? PasteTime : NewNotify.GetTime();
		NewNotifyTime = FMath::Clamp(NewNotifyTime, 0.0f, static_cast<float>(MotionAnim->GetPlayLength()));

		NewNotify.Link(MotionSequence, PasteTime, NewSlotIndex);

		NewNotify.TriggerTimeOffset = GetTriggerTimeOffsetForType(MotionSequence->CalculateOffsetForNotify(NewNotify.GetTime()));
		NewNotify.TrackIndex = TrackIndex;

		bool bValidNotify = true;
		if (NewNotify.Notify)
		{
			MotionAnim->Modify();
			
			UAnimNotify* NewNotifyObject = Cast<UAnimNotify>(StaticDuplicateObject(NewNotify.Notify, MotionAnim)); //TODO: Should be replaced with MotionDataAsset
			check(NewNotifyObject);
			bValidNotify = NewNotifyObject->CanBePlaced(MotionSequence);
			NewNotify.Notify = NewNotifyObject;
		}
		else if (NewNotify.NotifyStateClass)
		{
			MotionAnim->Modify();
			
			UAnimNotifyState* NewNotifyStateObject = Cast<UAnimNotifyState>(StaticDuplicateObject(NewNotify.NotifyStateClass, MotionAnim));  //TODO: Should be replaced with MotionDataAsset
			check(NewNotifyStateObject);
			NewNotify.NotifyStateClass = NewNotifyStateObject;
			bValidNotify = NewNotifyStateObject->CanBePlaced(MotionSequence);
			// Clamp duration into the sequence
			NewNotify.SetDuration(FMath::Clamp(NewNotify.GetDuration(), 1 / 30.0f, (float)MotionAnim->GetPlayLength() - NewNotify.GetTime()));
			NewNotify.EndTriggerTimeOffset = GetTriggerTimeOffsetForType(MotionSequence->CalculateOffsetForNotify(NewNotify.GetTime() + NewNotify.GetDuration()));
			NewNotify.EndLink.Link(MotionSequence, NewNotify.EndLink.GetTime()); 
		}

		NewNotify.Guid = FGuid::NewGuid();

		if (!bValidNotify)
		{
			// Paste failed, remove the notify
			MotionAnim->Tags.RemoveAt(NewIndex);

			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToPaste", "The tag is not allowed to be in this asset."));
		}
	}
	else
	{
		// Paste failed, remove the notify
		MotionAnim->Tags.RemoveAt(NewIndex);
	}

	OnDeselectAllTags.ExecuteIfBound();
	OnUpdatePanel.ExecuteIfBound();
}

void SMotionTagTrack::AppendSelectedNodeWidgetsToArray(TArray<TSharedPtr<SMotionTagNode>>& NodeArray) const
{
	for (TSharedPtr<SMotionTagNode> Node : NotifyNodes)
	{
		if (Node->bSelected)
		{
			NodeArray.Add(Node);
		}
	}
}

void SMotionTagTrack::RefreshMarqueeSelectedNodes(FSlateRect& Rect, FTagMarqueeOperation& Marquee)
{
	if (Marquee.Operation != FTagMarqueeOperation::Replace)
	{
		// Maintain the original selection from before the operation
		for (int32 Idx = 0; Idx < NotifyNodes.Num(); ++Idx)
		{
			TSharedPtr<SMotionTagNode> Notify = NotifyNodes[Idx];
			bool bWasSelected = Marquee.OriginalSelection.Contains(Notify);
			if (bWasSelected)
			{
				SelectTrackObjectNode(Idx, true, false);
			}
			else if (SelectedNodeIndices.Contains(Idx))
			{
				DeselectTrackObjectNode(Idx, false);
			}
		}
	}

	for (int32 Index = 0; Index < NotifyNodes.Num(); ++Index)
	{
		TSharedPtr<SMotionTagNode> Node = NotifyNodes[Index];
		FSlateRect NodeRect = FSlateRect(Node->GetWidgetPosition(), Node->GetWidgetPosition() + Node->GetSize());

		if (FSlateRect::DoRectanglesIntersect(Rect, NodeRect))
		{
			// Either select or deselect the intersecting node, depending on the type of selection operation
			if (Marquee.Operation == FTagMarqueeOperation::Remove)
			{
				if (SelectedNodeIndices.Contains(Index))
				{
					DeselectTrackObjectNode(Index, false);
				}
			}
			else
			{
				SelectTrackObjectNode(Index, true, false);
			}
		}
	}
}

FString SMotionTagTrack::MakeBlueprintNotifyName(const FString& InNotifyClassName)
{
	FString DefaultNotifyName = InNotifyClassName;
	DefaultNotifyName = DefaultNotifyName.Replace(TEXT("AnimNotify_"), TEXT(""), ESearchCase::CaseSensitive);
	DefaultNotifyName = DefaultNotifyName.Replace(TEXT("AnimNotifyState_"), TEXT(""), ESearchCase::CaseSensitive);

	return DefaultNotifyName;
}

void SMotionTagTrack::ClearNodeTooltips()
{
	FText EmptyTooltip;

	for (TSharedPtr<SMotionTagNode> Node : NotifyNodes)
	{
		Node->SetToolTipText(EmptyTooltip);
	}
}

const EVisibility SMotionTagTrack::GetTimingNodeVisibility(TSharedPtr<SMotionTagNode> NotifyNode)
{
	if (OnGetTimingNodeVisibility.IsBound())
	{
		if (FAnimNotifyEvent* Event = NotifyNode->NodeObjectInterface->GetNotifyEvent())
		{
			return Event->IsBranchingPoint() ? OnGetTimingNodeVisibility.Execute(ETimingElementType::BranchPointNotify) : OnGetTimingNodeVisibility.Execute(ETimingElementType::QueuedNotify);
		}
	}

	// No visibility defined, not visible
	return EVisibility::Hidden;
}

void SMotionTagTrack::UpdateCachedGeometry(const FGeometry& InGeometry)
{
	CachedGeometry = InGeometry;

	for (TSharedPtr<SMotionTagNode> Node : NotifyNodes)
	{
		Node->CachedTrackGeometry = InGeometry;
	}
}

//////////////////////////////////////////////////////////////////////////
// SSequenceEdTrack

void STagEdTrack::Construct(const FArguments& InArgs)
{
	MotionAnim = InArgs._MotionAnim;
	TrackIndex = InArgs._TrackIndex;
	FAnimNotifyTrack& Track = MotionAnim->MotionTagTracks[InArgs._TrackIndex];
	Track.TrackColor = ((TrackIndex & 1) != 0) ? FLinearColor(0.9f, 0.9f, 0.9f, 0.9f) : FLinearColor(0.5f, 0.5f, 0.5f);

	TSharedRef<SMotionTagPanel> PanelRef = InArgs._AnimNotifyPanel.ToSharedRef();
	AnimPanelPtr = InArgs._AnimNotifyPanel;

	//////////////////////////////
	this->ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				// Notification editor panel
				SAssignNew(NotifyTrack, SMotionTagTrack)
				.MotionAnim(MotionAnim)
				.TrackIndex(TrackIndex)
				.AnimNotifies(Track.Notifies)
				.ViewInputMin(InArgs._ViewInputMin)
				.ViewInputMax(InArgs._ViewInputMax)
				.OnSelectionChanged(InArgs._OnSelectionChanged)
				.OnUpdatePanel(InArgs._OnUpdatePanel)
				.OnGetTagBlueprintData(InArgs._OnGetTagBlueprintData)
				.OnGetTagStateBlueprintData(InArgs._OnGetTagStateBlueprintData)
				.OnGetTagNativeClasses(InArgs._OnGetTagNativeClasses)
				.OnGetTagStateNativeClasses(InArgs._OnGetTagStateNativeClasses)
				.OnGetScrubValue(InArgs._OnGetScrubValue)
				.OnGetDraggedNodePos(InArgs._OnGetDraggedNodePos)
				.OnNodeDragStarted(InArgs._OnNodeDragStarted)
				.OnSnapPosition(InArgs._OnSnapPosition)
				.TrackColor(Track.TrackColor)
				.OnRequestTrackPan(FPanTrackRequest::CreateSP(PanelRef, &SMotionTagPanel::PanInputViewRange))
				.OnRequestOffsetRefresh(InArgs._OnRequestRefreshOffsets)
				.OnDeleteTag(InArgs._OnDeleteTag)
				.OnGetIsMotionTagSelectionValidForReplacement(PanelRef, &SMotionTagPanel::IsTagSelectionValidForReplacement)
				.OnReplaceSelectedWithTag(PanelRef, &SMotionTagPanel::OnReplaceSelectedWithTag)
				.OnReplaceSelectedWithBlueprintTag(PanelRef, &SMotionTagPanel::OnReplaceSelectedWithTagBlueprint)
				.OnDeselectAllTags(InArgs._OnDeselectAllTags)
				.OnCopyNodes(InArgs._OnCopyNodes)
				.OnPasteNodes(InArgs._OnPasteNodes)
				.OnSetInputViewRange(InArgs._OnSetInputViewRange)
				.OnGetTimingNodeVisibility(InArgs._OnGetTimingNodeVisibility)
				.OnInvokeTab(InArgs._OnInvokeTab)
				.CommandList(PanelRef->GetCommandList())
			]
		];
}

bool STagEdTrack::CanDeleteTrack()
{
	return AnimPanelPtr.Pin()->CanDeleteTrack(TrackIndex);
}

//////////////////////////////////////////////////////////////////////////
// FMotionTagPanelCommands

void FMotionTagPanelCommands::RegisterCommands()
{
	UI_COMMAND(DeleteTag, "Delete", "Deletes the selected Tags.", EUserInterfaceActionType::Button, FInputChord(EKeys::Platform_Delete));
	UI_COMMAND(CopyTags, "Copy", "Copy animation tag events.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::C));
	UI_COMMAND(PasteTags, "Paste", "Paste animation tag event here.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::V));
}

//////////////////////////////////////////////////////////////////////////
// SMotionTagPanel

void SMotionTagPanel::Construct(const FArguments& InArgs, const TSharedRef<FMotionModel>& InModel)
{
	SMotionTrackPanel::Construct(SMotionTrackPanel::FArguments()
		.WidgetWidth(InArgs._WidgetWidth)
		.ViewInputMin(InArgs._ViewInputMin)
		.ViewInputMax(InArgs._ViewInputMax)
		.InputMin(InArgs._InputMin)
		.InputMax(InArgs._InputMax)
		.OnSetInputViewRange(InArgs._OnSetInputViewRange));

	WeakModel = InModel;
	WeakCommandList = InModel->GetCommandList();
	OnInvokeTab = InArgs._OnInvokeTab;
	OnTagsChanged = InArgs._OnTagsChanged;
	OnSnapPosition = InArgs._OnSnapPosition;
	bIsSelecting = false;
	bIsUpdating = false;

	InModel->OnHandleObjectsSelected().AddSP(this, &SMotionTagPanel::HandleObjectsSelected);

	MotionAnim = InModel->MotionAnim;

	FMotionTagPanelCommands::Register();
	BindCommands();

	MotionAnim->RegisterOnTagChanged(UMotionAnimObject::FOnTagChanged::CreateSP(this, &SMotionTagPanel::RefreshTagTracks));

	InModel->OnTracksChanged().Add(FSimpleDelegate::CreateSP(this, &SMotionTagPanel::RefreshTagTracks));

	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}

	CurrentPosition = InArgs._CurrentPosition;
	OnSelectionChanged = InArgs._OnSelectionChanged;
	WidgetWidth = InArgs._WidgetWidth;
	OnGetScrubValue = InArgs._OnGetScrubValue;
	OnRequestRefreshOffsets = InArgs._OnRequestRefreshOffsets;
	OnGetTimingNodeVisibility = InArgs._OnGetTimingNodeVisibility;

	this->ChildSlot
		[
			SAssignNew(PanelArea, SBorder)
			.Visibility(EVisibility::SelfHitTestInvisible)
			.AddMetaData<FTagMetaData>(TEXT("AnimNotify.Notify"))
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			.Padding(0.0f)
			.ColorAndOpacity(FLinearColor::White)
		];

	OnPropertyChangedHandle = FCoreUObjectDelegates::FOnObjectPropertyChanged::FDelegate::CreateSP(this, &SMotionTagPanel::OnPropertyChanged);
	OnPropertyChangedHandleDelegateHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.Add(OnPropertyChangedHandle);

	// Base notify classes used to search asset data for children.
	TagClassNames.Add(TEXT("Class'/Script/Engine.AnimNotify'"));
	TagStateClassNames.Add(TEXT("Class'/Script/Engine.AnimNotifyState'"));

	PopulateTagBlueprintClasses(TagClassNames);
	PopulateTagBlueprintClasses(TagStateClassNames);

	Update();
}

SMotionTagPanel::~SMotionTagPanel()
{
	//Sequence->UnregisterOnNotifyChanged(this);
	/*if (MotionAnim)
	{
		MotionAnim->UnRegisterOnTagChanged(this);
	}

	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnPropertyChangedHandleDelegateHandle);*/

	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}

FName SMotionTagPanel::GetNewTrackName() const
{
	TArray<FName> TrackNames;
	TrackNames.Reserve(50);

	for (const FAnimNotifyTrack& Track : MotionAnim->MotionTagTracks)
	{
		TrackNames.Add(Track.TrackName);
	}

	FName NameToTest;
	int32 TrackIndex = 1;

	do
	{
		NameToTest = *FString::FromInt(TrackIndex++);
	} while (TrackNames.Contains(NameToTest));

	return NameToTest;
}

FReply SMotionTagPanel::InsertTrack(int32 TrackIndexToInsert)
{
	MotionAnim->Modify();
	
	// before insert, make sure everything behind is fixed
	for (int32 I = TrackIndexToInsert; I < MotionAnim->MotionTagTracks.Num(); ++I)
	{
		FAnimNotifyTrack& Track = MotionAnim->MotionTagTracks[I];

		const int32 NewTrackIndex = I + 1;

		for (FAnimNotifyEvent* Notify : Track.Notifies)
		{
			// fix notifies indices
			Notify->TrackIndex = NewTrackIndex;
		}
	}

	FAnimNotifyTrack NewItem;
	NewItem.TrackName = GetNewTrackName();
	NewItem.TrackColor = FLinearColor::White;
	
	MotionAnim->MotionTagTracks.Insert(NewItem, TrackIndexToInsert);

	Update();

	return FReply::Handled();
}

FReply SMotionTagPanel::AddTrack()
{
	FAnimNotifyTrack NewItem;
	NewItem.TrackName = GetNewTrackName();
	NewItem.TrackColor = FLinearColor::White;

	MotionAnim->MotionTagTracks.Add(NewItem);
	MotionAnim->ParentMotionDataAsset->MarkPackageDirty();

	Update();

	return FReply::Handled();
}

FReply SMotionTagPanel::DeleteTrack(int32 TrackIndexToDelete)
{
	if (MotionAnim->MotionTagTracks.IsValidIndex(TrackIndexToDelete))
	{
		if (MotionAnim->MotionTagTracks[TrackIndexToDelete].Notifies.Num() == 0)
		{
			// before insert, make sure everything behind is fixed
			for (int32 I = TrackIndexToDelete + 1; I < MotionAnim->MotionTagTracks.Num(); ++I)
			{
				FAnimNotifyTrack& Track = MotionAnim->MotionTagTracks[I];
				const int32 NewTrackIndex = I - 1;

				for (FAnimNotifyEvent* Notify : Track.Notifies)
				{
					// fix notifies indices
					Notify->TrackIndex = NewTrackIndex;
				}
			}

			MotionAnim->MotionTagTracks.RemoveAt(TrackIndexToDelete);
			MotionAnim->ParentMotionDataAsset->PostEditChange();
			MotionAnim->ParentMotionDataAsset->MarkPackageDirty();
			Update();
		}
	}
	return FReply::Handled();
}

bool SMotionTagPanel::CanDeleteTrack(int32 TrackIndexToDelete)
{
	if (MotionAnim->MotionTagTracks.Num() > 1 && MotionAnim->MotionTagTracks.IsValidIndex(TrackIndexToDelete))
	{
		return MotionAnim->MotionTagTracks[TrackIndexToDelete].Notifies.Num() == 0;
	}

	return false;
}

void SMotionTagPanel::OnCommitTrackName(const FText& InText, ETextCommit::Type CommitInfo, int32 TrackIndexToName)
{
	if (MotionAnim->MotionTagTracks.IsValidIndex(TrackIndexToName))
	{
		FScopedTransaction Transaction(FText::Format(LOCTEXT("RenameTagTrack", "Rename Tag Track to '{0}'"), InText));
		MotionAnim->ParentMotionDataAsset->Modify();

		FText TrimText = FText::TrimPrecedingAndTrailing(InText);
		MotionAnim->MotionTagTracks[TrackIndexToName].TrackName = FName(*TrimText.ToString());
	}
}

void SMotionTagPanel::Update()
{
	if (!bIsUpdating)
	{
		TGuardValue<bool> ScopeGuard(bIsUpdating, true);

		/*if (Sequence != NULL)
		{
			Sequence->RefreshCacheData();
		}*/

		if (MotionAnim != nullptr)
		{
			MotionAnim->RefreshCacheData();
		}

		RefreshTagTracks();

		OnTagsChanged.ExecuteIfBound();
	}
}

// Helper to save/restore selection state when widgets are recreated
struct FScopedSavedTagSelection
{
	FScopedSavedTagSelection(SMotionTagPanel& InPanel)
		: Panel(InPanel)
	{
		for (TSharedPtr<SMotionTagTrack> Track : InPanel.TagMotionTracks)
		{
			for (int32 NodeIndex = 0; NodeIndex < Track->GetNumNotifyNodes(); ++NodeIndex)
			{
				if (Track->IsNodeSelected(NodeIndex))
				{
					SelectedNodeGuids.Add(Track->GetNodeObjectInterface(NodeIndex)->GetGuid());
				}
			}
		}
	}

	~FScopedSavedTagSelection()
	{
		// Re-apply selection state
		for (TSharedPtr<SMotionTagTrack> Track : Panel.TagMotionTracks)
		{
			Track->SelectNodesByGuid(SelectedNodeGuids, false);
		}
	}

	SMotionTagPanel& Panel;
	TSet<FGuid> SelectedNodeGuids;
};

void SMotionTagPanel::RefreshTagTracks()
{
	//check(Sequence);
	check(MotionAnim);

	{
		FScopedSavedTagSelection ScopedSelection(*this);

		TSharedPtr<SVerticalBox> NotifySlots;
		PanelArea->SetContent(
			SAssignNew(NotifySlots, SVerticalBox)
		);

		// Clear node tool tips to stop slate referencing them and possibly
		// causing a crash if the notify has gone away
		for (TSharedPtr<SMotionTagTrack> Track : TagMotionTracks)
		{
			Track->ClearNodeTooltips();
		}

		TagMotionTracks.Empty();
		TagEditorTracks.Empty();

		for (int32 TrackIndex = 0; TrackIndex < MotionAnim->MotionTagTracks.Num(); TrackIndex++)
		{
			FAnimNotifyTrack& Track = MotionAnim->MotionTagTracks[TrackIndex];
			TSharedPtr<STagEdTrack> EdTrack;

			NotifySlots->AddSlot()
				.AutoHeight()
				.VAlign(VAlign_Center)
				[
					SAssignNew(EdTrack, STagEdTrack)
					.TrackIndex(TrackIndex)
					.MotionAnim(MotionAnim)
					//.Sequence(Cast<UAnimSequenceBase>(MotionAnim->AnimAsset))
					.AnimNotifyPanel(SharedThis(this))
					.WidgetWidth(WidgetWidth)
					.ViewInputMin(ViewInputMin)
					.ViewInputMax(ViewInputMax)
					.OnGetScrubValue(OnGetScrubValue)
					.OnGetDraggedNodePos(this, &SMotionTagPanel::CalculateDraggedNodePos)
					.OnUpdatePanel(this, &SMotionTagPanel::Update)
					.OnGetTagBlueprintData(this, &SMotionTagPanel::OnGetTagBlueprintData, &TagClassNames)
					.OnGetTagStateBlueprintData(this, &SMotionTagPanel::OnGetTagBlueprintData, &TagStateClassNames)
					.OnGetTagNativeClasses(this, &SMotionTagPanel::OnGetNativeTagData, UAnimNotify::StaticClass(), &TagClassNames)
					.OnGetTagStateNativeClasses(this, &SMotionTagPanel::OnGetNativeTagData, UAnimNotifyState::StaticClass(), &TagStateClassNames)
					.OnSelectionChanged(this, &SMotionTagPanel::OnTrackSelectionChanged)
					.OnNodeDragStarted(this, &SMotionTagPanel::OnTagNodeDragStarted)
					.OnSnapPosition(OnSnapPosition)
					.OnRequestRefreshOffsets(OnRequestRefreshOffsets)
					.OnDeleteTag(this, &SMotionTagPanel::DeleteSelectedNodeObjects)
					.OnDeselectAllTags(this, &SMotionTagPanel::DeselectAllTags)
					.OnCopyNodes(this, &SMotionTagPanel::CopySelectedNodesToClipboard)
					.OnPasteNodes(this, &SMotionTagPanel::OnPasteNodes)
					.OnSetInputViewRange(this, &SMotionTagPanel::InputViewRangeChanged)
					.OnGetTimingNodeVisibility(OnGetTimingNodeVisibility)
					.OnInvokeTab(OnInvokeTab)
				];

			TagMotionTracks.Add(EdTrack->NotifyTrack);
			TagEditorTracks.Add(EdTrack);
		}
	}

	// Signal selection change to refresh details panel
	OnTrackSelectionChanged();
}

float SMotionTagPanel::CalculateDraggedNodePos() const
{
	return CurrentDragXPosition;
}

FReply SMotionTagPanel::OnTagNodeDragStarted(TArray<TSharedPtr<SMotionTagNode>> NotifyNodes, TSharedRef<SWidget> Decorator, 
	const FVector2D& ScreenCursorPos, const FVector2D& ScreenNodePosition, const bool bDragOnMarker)
{
	TSharedRef<SOverlay> NodeDragDecoratorOverlay = SNew(SOverlay);
	TSharedRef<SBorder> NodeDragDecorator = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			NodeDragDecoratorOverlay
		];

	TArray<TSharedPtr<SMotionTagNode>> Nodes;
	
	for (int32 i = 0; i < TagMotionTracks.Num(); ++i)
	{
		TSharedPtr<SMotionTagTrack> Track = TagMotionTracks[i];
		Track->DisconnectSelectedNodesForDrag(Nodes);
	}

	FBox2D OverlayBounds(Nodes[0]->GetScreenPosition(), Nodes[0]->GetScreenPosition() + FVector2D(Nodes[0]->GetDurationSize(), 0.0f));
	for (int32 Idx = 1; Idx < Nodes.Num(); ++Idx)
	{
		TSharedPtr<SMotionTagNode> Node = Nodes[Idx];
		FVector2D NodePosition = Node->GetScreenPosition();
		float NodeDuration = Node->GetDurationSize();

		OverlayBounds += FBox2D(NodePosition, NodePosition + FVector2D(NodeDuration, 0.0f));
	}

	FVector2D OverlayOrigin = OverlayBounds.Min;
	FVector2D OverlayExtents = OverlayBounds.GetSize();

	for (TSharedPtr<SMotionTagNode> Node : Nodes)
	{
		FVector2D OffsetFromFirst(Node->GetScreenPosition() - OverlayOrigin);

		NodeDragDecoratorOverlay->AddSlot()
			.Padding(FMargin(OffsetFromFirst.X, OffsetFromFirst.Y, 0.0f, 0.0f))
			[
				Node->AsShared()
			];
	}

	FPanTrackRequest PanRequestDelegate = FPanTrackRequest::CreateSP(this, &SMotionTagPanel::PanInputViewRange);
	FOnUpdatePanel UpdateDelegate = FOnUpdatePanel::CreateSP(this, &SMotionTagPanel::Update);
	return FReply::Handled().BeginDragDrop(FTagDragDropOp::New(Nodes, NodeDragDecorator, 
		TagMotionTracks, MotionAnim, ScreenCursorPos, OverlayOrigin, OverlayExtents, CurrentDragXPosition, 
		PanRequestDelegate, OnSnapPosition, UpdateDelegate));
}

float SMotionTagPanel::GetSequenceLength() const
{
	return MotionAnim->GetPlayLength();
}

void SMotionTagPanel::PostUndo(bool bSuccess)
{
	/*if (Sequence != NULL)
	{
		Sequence->RefreshCacheData();
	}*/

	if (MotionAnim != nullptr)
	{
		MotionAnim->RefreshCacheData();
	}
}

void SMotionTagPanel::PostRedo(bool bSuccess)
{
	/*if (Sequence != NULL)
	{
		Sequence->RefreshCacheData();
	}*/

	if (MotionAnim != nullptr)
	{
		MotionAnim->RefreshCacheData();
	}
}

void SMotionTagPanel::OnDeletePressed()
{
	// If there's no focus on the panel it's likely the user is not editing notifies
	// so don't delete anything when the key is pressed.
	if (HasKeyboardFocus() || HasFocusedDescendants())
	{
		DeleteSelectedNodeObjects();
	}
}

void SMotionTagPanel::DeleteSelectedNodeObjects()
{
	TArray<INodeObjectInterface*> SelectedNodes;
	for (TSharedPtr<SMotionTagTrack> Track : TagMotionTracks)
	{
		Track->AppendSelectionToArray(SelectedNodes);
	}

	const bool bContainsSyncMarkers = false; //SelectedNodes.ContainsByPredicate([](const INodeObjectInterface* Interface) { return Interface->GetType() == ENodeObjectTypes::NOTIFY; });

	if (SelectedNodes.Num() > 0)
	{
		MotionAnim->Modify();
		
		FScopedTransaction Transaction(LOCTEXT("DeleteMarkers", "Delete Animation Markers"));
		// As we address node object's source data by pointer, we need to mark for delete then
		// delete invalid entries to avoid concurrent modification of containers
		for (INodeObjectInterface* NodeObject : SelectedNodes)
		{
			NodeObject->MarkForDelete(MotionAnim);
		}

		FTagNodeInterface::RemoveInvalidNotifies(MotionAnim);
	}

	// clear selection and update the panel
	TArray<UObject*> Objects;
	OnSelectionChanged.ExecuteIfBound(Objects);

	Update();
}

void SMotionTagPanel::SetSequence(TObjectPtr<UMotionAnimObject> InMotionAnim)
{
	if (InMotionAnim != MotionAnim)
	{
		MotionAnim = InMotionAnim;
		MotionAnim->InitializeTagTrack();
		Update();
	}
}

void SMotionTagPanel::OnTrackSelectionChanged()
{
	if (!bIsSelecting)
	{
		TGuardValue<bool> GuardValue(bIsSelecting, true);

		// Need to collect selection info from all tracks
		TArray<UObject*> NotifyObjects;

		for (int32 TrackIndex = 0; TrackIndex < TagMotionTracks.Num(); ++TrackIndex)
		{
			TSharedPtr<SMotionTagTrack> Track = TagMotionTracks[TrackIndex];
			const TArray<int32>& TrackIndices = Track->GetSelectedNotifyIndices();
			for (int32 Index : TrackIndices)
			{
				INodeObjectInterface* NodeObjectInterface = Track->GetNodeObjectInterface(Index);
				if (FAnimNotifyEvent* NotifyEvent = NodeObjectInterface->GetNotifyEvent())
				{
					FString ObjName = MakeUniqueObjectName(GetTransientPackage(), UEditorNotifyObject::StaticClass()).ToString();
					UEditorNotifyObject* NewNotifyObject = NewObject<UEditorNotifyObject>(GetTransientPackage(), FName(*ObjName), RF_Public | RF_Standalone | RF_Transient);
					NewNotifyObject->InitFromAnim(Cast<UAnimSequenceBase>(MotionAnim->AnimAsset), FOnAnimObjectChange::CreateSP(this, &SMotionTagPanel::OnTagObjectChanged));
					NewNotifyObject->InitialiseNotify(*MotionAnim->MotionTagTracks[TrackIndex].Notifies[Index]);
					NotifyObjects.AddUnique(NewNotifyObject);
				}
			}
		}

		OnSelectionChanged.ExecuteIfBound(NotifyObjects);
	}
}

void SMotionTagPanel::DeselectAllTags()
{
	if (!bIsSelecting)
	{
		TGuardValue<bool> GuardValue(bIsSelecting, true);

		for (int32 i = 0; i < TagMotionTracks.Num(); ++i)
		{
			TSharedPtr<SMotionTagTrack> Track = TagMotionTracks[i];
			Track->DeselectAllNotifyNodes(false);
		}

		TArray<UObject*> NotifyObjects;
		OnSelectionChanged.ExecuteIfBound(NotifyObjects);
	}
}

void SMotionTagPanel::CopySelectedNodesToClipboard() const
{
	// Grab the selected events
	TArray<INodeObjectInterface*> SelectedNodes;
	for (const TSharedPtr<SMotionTagTrack> Track : TagMotionTracks)
	{
		Track->AppendSelectionToArray(SelectedNodes);
	}

	const FString HeaderString(TEXT("COPY_ANIMNOTIFYEVENT"));

	if (SelectedNodes.Num() > 0)
	{
		FString StrValue(HeaderString);

		// Sort by track
		SelectedNodes.Sort([](const INodeObjectInterface& A, const INodeObjectInterface& B)
			{
				return (A.GetTrackIndex() < B.GetTrackIndex()) || (A.GetTrackIndex() == B.GetTrackIndex() && A.GetTime() < B.GetTime());
			});

		// Need to find how many tracks this selection spans and the minimum time to use as the beginning of the selection
		int32 MinTrack = MAX_int32;
		int32 MaxTrack = MIN_int32;
		float MinTime = MAX_flt;
		for (const INodeObjectInterface* NodeObject : SelectedNodes)
		{
			MinTrack = FMath::Min(MinTrack, NodeObject->GetTrackIndex());
			MaxTrack = FMath::Max(MaxTrack, NodeObject->GetTrackIndex());
			MinTime = FMath::Min(MinTime, NodeObject->GetTime());
		}
		
		const int32 TrackSpan = MaxTrack - MinTrack + 1;

		StrValue += FString::Printf(TEXT("OriginalTime=%f,"), MinTime);
		StrValue += FString::Printf(TEXT("OriginalLength=%f,"), MotionAnim->GetPlayLength());
		StrValue += FString::Printf(TEXT("TrackSpan=%d"), TrackSpan);

		for (const INodeObjectInterface* NodeObject : SelectedNodes)
		{
			// Locate the notify in the sequence, we need the sequence index; but also need to
			// keep the order we're currently in.

			StrValue += "\n";
			StrValue += FString::Printf(TEXT("AbsTime=%f,NodeObjectType=%i,"), NodeObject->GetTime(), static_cast<int32>(NodeObject->GetType()));

			NodeObject->ExportForCopy(MotionAnim, StrValue);
		}

		FPlatformApplicationMisc::ClipboardCopy(*StrValue);
	}
}

bool SMotionTagPanel::IsTagSelectionValidForReplacement()
{
	// Grab the selected events
	TArray<INodeObjectInterface*> SelectedNodes;

	for (int32 i = 0; i < TagMotionTracks.Num(); ++i)
	{
		TSharedPtr<SMotionTagTrack> Track = TagMotionTracks[i];
		Track->AppendSelectionToArray(SelectedNodes);
	}

	bool bSelectionContainsAnimNotify = false;
	bool bSelectionContainsAnimNotifyState = false;
	for (INodeObjectInterface* NodeObject : SelectedNodes)
	{
		FAnimNotifyEvent* NotifyEvent = NodeObject->GetNotifyEvent();
		if (NotifyEvent)
		{
			if (NotifyEvent->Notify)
			{
				bSelectionContainsAnimNotify = true;
			}
			else if (NotifyEvent->NotifyStateClass)
			{
				bSelectionContainsAnimNotifyState = true;
			}
			// Custom AnimNotifies have no class, but they are like AnimNotify class notifies in that they have no duration
			else
			{
				bSelectionContainsAnimNotify = true;
			}
		}
	}

	// Only allow replacement for selections that contain _only_ AnimNotifies, or _only_ AnimNotifyStates, but not both
	// (Want to disallow replacement of AnimNotify with AnimNotifyState, and vice-versa)
	bool bIsValidSelection = bSelectionContainsAnimNotify != bSelectionContainsAnimNotifyState;

	return bIsValidSelection;
}


void SMotionTagPanel::OnReplaceSelectedWithTag(FString NewNotifyName, UClass* NewNotifyClass)
{
	TArray<INodeObjectInterface*> SelectedNodes;
	
	for (int32 i = 0; i < TagMotionTracks.Num(); ++i)
	{
		TSharedPtr<SMotionTagTrack> Track = TagMotionTracks[i];
		Track->AppendSelectionToArray(SelectedNodes);
	}

	// Sort these since order is important for deletion
	SelectedNodes.Sort();

	const FScopedTransaction Transaction(LOCTEXT("ReplaceAnimTag", "Replace Motion Tag"));

	MotionAnim->Modify(true);
	
	for (INodeObjectInterface* NodeObject : SelectedNodes)
	{
		FAnimNotifyEvent* OldEvent = NodeObject->GetNotifyEvent();
		if (OldEvent)
		{
			float BeginTime = OldEvent->GetTime();
			float Length = OldEvent->GetDuration();
			int32 TargetTrackIndex = OldEvent->TrackIndex;
			float TriggerTimeOffset = OldEvent->TriggerTimeOffset;
			float EndTriggerTimeOffset = OldEvent->EndTriggerTimeOffset;
			int32 SlotIndex = OldEvent->GetSlotIndex();
			int32 EndSlotIndex = OldEvent->EndLink.GetSlotIndex();
			int32 SegmentIndex = OldEvent->GetSegmentIndex();
			int32 EndSegmentIndex = OldEvent->GetSegmentIndex();
			EAnimLinkMethod::Type LinkMethod = OldEvent->GetLinkMethod();
			EAnimLinkMethod::Type EndLinkMethod = OldEvent->EndLink.GetLinkMethod();

			FColor OldColor = OldEvent->NotifyColor;
			UAnimNotify* OldEventPayload = OldEvent->Notify;

			// Delete old one before creating new one to avoid potential array re-allocation when array temporarily increases by 1 in size
			NodeObject->Delete(MotionAnim);
			FAnimNotifyEvent& NewEvent = TagMotionTracks[TargetTrackIndex]->CreateNewNotify(NewNotifyName, NewNotifyClass, BeginTime);

			NewEvent.TriggerTimeOffset = TriggerTimeOffset;
			NewEvent.ChangeSlotIndex(SlotIndex);
			NewEvent.SetSegmentIndex(SegmentIndex);
			NewEvent.ChangeLinkMethod(LinkMethod);
			NewEvent.NotifyColor = OldColor;

			// Copy what we can across from the payload
			if ((OldEventPayload != nullptr) && (NewEvent.Notify != nullptr))
			{
				UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
				CopyParams.bNotifyObjectReplacement = true;
				UEngine::CopyPropertiesForUnrelatedObjects(OldEventPayload, NewEvent.Notify, CopyParams);
			}

			// For Motion Tag Sections, handle the end time and link
			if (NewEvent.NotifyStateClass != nullptr)
			{
				NewEvent.SetDuration(Length);
				NewEvent.EndTriggerTimeOffset = EndTriggerTimeOffset;
				NewEvent.EndLink.ChangeSlotIndex(EndSlotIndex);
				NewEvent.EndLink.SetSegmentIndex(EndSegmentIndex);
				NewEvent.EndLink.ChangeLinkMethod(EndLinkMethod);
			}

			NewEvent.Update();
		}
	}

	// clear selection  
	TArray<UObject*> Objects;
	OnSelectionChanged.ExecuteIfBound(Objects);

	Update();
}

void SMotionTagPanel::OnReplaceSelectedWithTagBlueprint(FString NewBlueprintNotifyName, FString NewBlueprintNotifyClass)
{
	TSubclassOf<UObject> BlueprintClass = SMotionTagTrack::GetBlueprintClassFromPath(NewBlueprintNotifyClass);
	OnReplaceSelectedWithTag(NewBlueprintNotifyName, BlueprintClass);
}

void SMotionTagPanel::OnPasteNodes(SMotionTagTrack* RequestTrack, float ClickTime, ETagPasteMode::Type PasteMode, ETagPasteMultipleMode::Type MultiplePasteType)
{
	if (RequestTrack == nullptr)
	{
		for (TSharedPtr<SMotionTagTrack> Track : TagMotionTracks)
		{
			if (Track->HasKeyboardFocus())
			{
				RequestTrack = Track.Get();
				if (ClickTime == -1.0f)
				{
					ClickTime = RequestTrack->GetLastClickedTime();
				}
				break;
			}
		}
	}

	const int32 PasteIdx = RequestTrack != nullptr ? RequestTrack->GetTrackIndex() : 0;
	int32 NumTracks = TagMotionTracks.Num();
	FString PropString;
	const TCHAR* Buffer;
	float OrigBeginTime;
	float OrigLength;
	int32 TrackSpan;
	int32 FirstTrack = -1;
	float ScaleMultiplier = 1.0f;

	if (ReadNotifyPasteHeader(PropString, Buffer, OrigBeginTime, OrigLength, TrackSpan))
	{
		DeselectAllTags();

		FScopedTransaction Transaction(LOCTEXT("PasteNotifyEvent", "Paste Motion Tags"));
		
		if (ClickTime == -1.0f)
		{
			if (PasteMode == ETagPasteMode::OriginalTime)
			{
				// We want to place the notifies exactly where they were
				ClickTime = OrigBeginTime;
			}
			else
			{
				ClickTime = WeakModel.Pin()->GetScrubTime();
			}
		}

		// Expand the number of tracks if we don't have enough.
		check(TrackSpan > 0);
		if (PasteIdx + TrackSpan > NumTracks)
		{
			int32 TracksToAdd = (PasteIdx + TrackSpan) - NumTracks;
			while (TracksToAdd)
			{
				AddTrack();
				--TracksToAdd;
			}
			RefreshTagTracks();
			NumTracks = TagMotionTracks.Num();
		}

		// Scaling for relative paste
		if (MultiplePasteType == ETagPasteMultipleMode::Relative)
		{
			ScaleMultiplier = MotionAnim->GetPlayLength() / OrigLength;
		}

		// Process each line of the paste buffer and spawn notifies
		FString CurrentLine;
		while (FParse::Line(&Buffer, CurrentLine))
		{
			int32 OriginalTrack;
			float OrigTime;
			int32 NodeObjectType;
			float PasteTime = -1.0f;
			if (FParse::Value(*CurrentLine, TEXT("TrackIndex="), OriginalTrack) && FParse::Value(*CurrentLine, TEXT("AbsTime="), OrigTime) && FParse::Value(*CurrentLine, TEXT("NodeObjectType="), NodeObjectType))
			{
				const int32 FirstComma = CurrentLine.Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromStart);
				const int32 SecondComma = CurrentLine.Find(TEXT(","), ESearchCase::CaseSensitive, ESearchDir::FromStart, FirstComma + 1);
				FString NotifyExportString = CurrentLine.RightChop(SecondComma + 1);

				// Store the first track so we know where to place notifies
				if (FirstTrack < 0)
				{
					FirstTrack = OriginalTrack;
				}
				const int32 TrackOffset = OriginalTrack - FirstTrack;
				const float TimeOffset = OrigTime - OrigBeginTime;
				const float TimeToPaste = ClickTime + TimeOffset * ScaleMultiplier;

				if(PasteIdx + TrackOffset < TagMotionTracks.Num())
				{
					MotionAnim->Modify();
					const TSharedPtr<SMotionTagTrack> TrackToUse = TagMotionTracks[PasteIdx + TrackOffset];
					TrackToUse->PasteSingleNotify(NotifyExportString, TimeToPaste);
				}
			}
		}
	}
}

void SMotionTagPanel::OnPropertyChanged(UObject* ChangedObject, FPropertyChangedEvent& PropertyEvent)
{
	// Bail if it isn't a notify
	if (!ChangedObject->GetClass()->IsChildOf(UAnimNotify::StaticClass()) &&
		!ChangedObject->GetClass()->IsChildOf(UAnimNotifyState::StaticClass()))
	{
		return;
	}

	// Don't process if it's an interactive change; wait till we receive the final event.
	if (PropertyEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		for (FAnimNotifyEvent& Event : MotionAnim->Tags)
		{
			if (Event.Notify == ChangedObject || Event.NotifyStateClass == ChangedObject)
			{
				// If we've changed a notify present in the sequence, refresh our tracks.
				Update();
			}
		}
	}
}

void SMotionTagPanel::BindCommands()
{
	TSharedRef<FUICommandList> CommandList = GetCommandList();
	const FMotionTagPanelCommands& Commands = FMotionTagPanelCommands::Get();

	CommandList->MapAction(
		Commands.DeleteTag,
		FExecuteAction::CreateSP(this, &SMotionTagPanel::OnDeletePressed));

	CommandList->MapAction(
		Commands.CopyTags,
		FExecuteAction::CreateSP(this, &SMotionTagPanel::CopySelectedNodesToClipboard));

	CommandList->MapAction(
		Commands.PasteTags,
		FExecuteAction::CreateSP(this, &SMotionTagPanel::OnPasteNodes, static_cast<SMotionTagTrack*>(nullptr), -1.0f, ETagPasteMode::MousePosition, ETagPasteMultipleMode::Absolute));
}

FReply SMotionTagPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (GetCommandList()->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SMotionTagPanel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SMotionTrackPanel::OnMouseButtonDown(MyGeometry, MouseEvent);

	bool bLeftButton = MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);

	if (bLeftButton)
	{
		TArray<TSharedPtr<SMotionTagNode>> SelectedNodes;
		for (TSharedPtr<SMotionTagTrack> Track : TagMotionTracks)
		{
			Track->AppendSelectedNodeWidgetsToArray(SelectedNodes);
		}

		Marquee.Start(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()), Marquee.OperationTypeFromMouseEvent(MouseEvent), SelectedNodes);
		if (Marquee.Operation == FTagMarqueeOperation::Replace)
		{
			// Remove and Add operations preserve selections, replace starts afresh
			DeselectAllTags();
		}

		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}

	return FReply::Unhandled();
}

FReply SMotionTagPanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (Marquee.bActive)
	{
		OnTrackSelectionChanged();
		Marquee = FTagMarqueeOperation();
		return FReply::Handled().ReleaseMouseCapture();
	}

	return SMotionTrackPanel::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SMotionTagPanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FReply BaseReply = SMotionTrackPanel::OnMouseMove(MyGeometry, MouseEvent);
	if (!BaseReply.IsEventHandled())
	{
		bool bLeftButton = MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);

		if (bLeftButton && Marquee.bActive)
		{
			Marquee.Rect.UpdateEndPoint(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
			RefreshMarqueeSelectedNodes(MyGeometry);
			return FReply::Handled();
		}
	}

	return BaseReply;
}

int32 SMotionTagPanel::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = SMotionTrackPanel::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	FVector2D Origin = AllottedGeometry.AbsoluteToLocal(Marquee.Rect.GetUpperLeft());
	FVector2D Extents = AllottedGeometry.AbsoluteToLocal(Marquee.Rect.GetSize());

	if (Marquee.IsValid())
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(Marquee.Rect.GetSize(), FSlateLayoutTransform(1.0f, Marquee.Rect.GetUpperLeft())),
			FAppStyle::GetBrush(TEXT("MarqueeSelection"))
		);
	}

	return LayerId;
}

void SMotionTagPanel::RefreshMarqueeSelectedNodes(const FGeometry& PanelGeo)
{
	if (Marquee.IsValid())
	{
		FSlateRect MarqueeRect = Marquee.Rect.ToSlateRect();
		for (TSharedPtr<SMotionTagTrack> Track : TagMotionTracks)
		{
			if (Marquee.Operation == FTagMarqueeOperation::Replace || Marquee.OriginalSelection.Num() == 0)
			{
				Track->DeselectAllNotifyNodes(false);
			}

			const FGeometry& TrackGeo = Track->GetCachedGeometry();

			FSlateRect TrackClip = TrackGeo.GetLayoutBoundingRect();
			FSlateRect PanelClip = PanelGeo.GetLayoutBoundingRect();
			FVector2D PanelSpaceOrigin = TrackClip.GetTopLeft() - PanelClip.GetTopLeft();
			FVector2D TrackSpaceOrigin = MarqueeRect.GetTopLeft() - PanelSpaceOrigin;
			FSlateRect MarqueeTrackSpace(TrackSpaceOrigin, TrackSpaceOrigin + MarqueeRect.GetSize());

			Track->RefreshMarqueeSelectedNodes(MarqueeTrackSpace, Marquee);
		}
	}
}

FReply SMotionTagPanel::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Marquee.bActive = true;
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

void SMotionTagPanel::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	if (Marquee.bActive)
	{
		OnTrackSelectionChanged();
	}
	Marquee = FTagMarqueeOperation();
}

void SMotionTagPanel::PopulateTagBlueprintClasses(TArray<FString>& InOutAllowedClasses)
{
	TArray<FAssetData> TempArray;
	OnGetTagBlueprintData(TempArray, &InOutAllowedClasses);
}

void SMotionTagPanel::OnGetTagBlueprintData(TArray<FAssetData>& OutNotifyData, TArray<FString>* InOutAllowedClassNames)
{
	// If we have nothing to seach with, early out
	if (InOutAllowedClassNames == NULL || InOutAllowedClassNames->Num() == 0)
	{
		return;
	}

	TArray<FAssetData> AssetDataList;
	TArray<FString> FoundClasses;

	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Collect a full list of assets with the specified class
	AssetRegistryModule.Get().GetAssetsByPath(UBlueprint::StaticClass()->GetFName(), AssetDataList);
	
	int32 BeginClassCount = InOutAllowedClassNames->Num();
	int32 CurrentClassCount = -1;

	while (BeginClassCount != CurrentClassCount)
	{
		BeginClassCount = InOutAllowedClassNames->Num();

		for (int32 AssetIndex = 0; AssetIndex < AssetDataList.Num(); ++AssetIndex)
		{
			FAssetData& AssetData = AssetDataList[AssetIndex];
			FString TagValue = AssetData.GetTagValueRef<FString>(FBlueprintTags::ParentClassPath);

			if (InOutAllowedClassNames->Contains(TagValue))
			{
				FString GenClass = AssetData.GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath);
				const uint32 ClassFlags = AssetData.GetTagValueRef<uint32>(FBlueprintTags::ClassFlags);
				if (ClassFlags & CLASS_Abstract)
				{
					continue;
				}

				if (!OutNotifyData.Contains(AssetData))
				{
					// Output the assetdata and record it as found in this request
					OutNotifyData.Add(AssetData);
					FoundClasses.Add(GenClass);
				}

				if (!InOutAllowedClassNames->Contains(GenClass))
				{
					// Expand the class list to account for a new possible parent class found
					InOutAllowedClassNames->Add(GenClass);
				}
			}
		}

		CurrentClassCount = InOutAllowedClassNames->Num();
	}

	// Count native classes, so we don't remove them from the list
	int32 NumNativeClasses = 0;
	for (FString& AllowedClass : *InOutAllowedClassNames)
	{
		if (!AllowedClass.EndsWith(FString(TEXT("_C'"))))
		{
			++NumNativeClasses;
		}
	}

	if (FoundClasses.Num() < InOutAllowedClassNames->Num() - NumNativeClasses)
	{
		// Less classes found, some may have been deleted or reparented
		for (int32 ClassIndex = InOutAllowedClassNames->Num() - 1; ClassIndex >= 0; --ClassIndex)
		{
			FString& ClassName = (*InOutAllowedClassNames)[ClassIndex];
			if (ClassName.EndsWith(FString(TEXT("_C'"))) && !FoundClasses.Contains(ClassName))
			{
				InOutAllowedClassNames->RemoveAt(ClassIndex);
			}
		}
	}
}

void SMotionTagPanel::OnGetNativeTagData(TArray<UClass*>& OutClasses, UClass* NotifyOutermost, TArray<FString>* OutAllowedBlueprintClassNames)
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;

		if (Class->IsChildOf(NotifyOutermost) && Class->HasAllClassFlags(CLASS_Native) && !Class->IsInBlueprint())
		{
			OutClasses.Add(Class);
			// Form class name to search later
			FString ClassName = FString::Printf(TEXT("%s'%s'"), *Class->GetClass()->GetName(), *Class->GetPathName());
			OutAllowedBlueprintClassNames->AddUnique(ClassName);
		}
	}
}

void SMotionTagPanel::OnTagObjectChanged(UObject* EditorBaseObj, bool bRebuild)
{
	if (UEditorNotifyObject* NotifyObject = Cast<UEditorNotifyObject>(EditorBaseObj))
	{
		FScopedSavedTagSelection ScopedSelection(*this);

		for (FAnimNotifyEvent& ThisTag : MotionAnim->Tags)
		{
			if (ThisTag.Guid == NotifyObject->Event.Guid)
			{
				if (TagMotionTracks.IsValidIndex(ThisTag.TrackIndex))
				{
					TagMotionTracks[ThisTag.TrackIndex]->Update();
				}
			}
		}
	}
}

void SMotionTagPanel::OnTagTrackScrolled(float InScrollOffsetFraction)
{
	float Ratio = (ViewInputMax.Get() - ViewInputMin.Get()) / MotionAnim->GetPlayLength();
	float MaxOffset = (Ratio < 1.0f) ? 1.0f - Ratio : 0.0f;
	InScrollOffsetFraction = FMath::Clamp(InScrollOffsetFraction, 0.0f, MaxOffset);

	// Calculate new view ranges
	float NewMin = InScrollOffsetFraction * MotionAnim->GetPlayLength();
	float NewMax = (InScrollOffsetFraction + Ratio) * MotionAnim->GetPlayLength();

	InputViewRangeChanged(NewMin, NewMax);
}

void SMotionTagPanel::InputViewRangeChanged(float ViewMin, float ViewMax)
{
	float Ratio = (ViewMax - ViewMin) / MotionAnim->GetPlayLength();
	float OffsetFraction = ViewMin / MotionAnim->GetPlayLength();
	if (TagTrackScrollBar.IsValid())
	{
		TagTrackScrollBar->SetState(OffsetFraction, Ratio);
	}

	SMotionTrackPanel::InputViewRangeChanged(ViewMin, ViewMax);
}

void SMotionTagPanel::HandleObjectsSelected(const TArray<UObject*>& InObjects)
{
	if (!bIsSelecting)
	{
		DeselectAllTags();
	}
}

#undef LOCTEXT_NAMESPACE