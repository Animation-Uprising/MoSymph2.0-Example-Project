//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Controls/MotionModel.h"

class SMotionOutlinerItem;
class SWidget;
enum class ECheckBoxState : uint8;
struct EVisibility;
class FMenuBuilder;
class SBorder;
class SHorizontalBox;

/** Structure used to define padding for a track */
struct FMotionTrackPadding
{
public:
	float Top;
	float Bottom;

public:
	FMotionTrackPadding(float InUniform)
		: Top(InUniform)
		, Bottom(InUniform) {}

	FMotionTrackPadding(float InTop, float InBottom)
		: Top(InTop)
		, Bottom(InBottom) {}

	float Combined() const
	{
		return Top + Bottom;
	}

};

// Simple RTTI implementation for tracks
#define MOTIONTIMELINE_DECLARE_BASE_TRACK(BaseClassName) \
	public: \
		static const FName& GetStaticTypeName() { return BaseClassName::TypeName; } \
		virtual const FName& GetTypeName() const { return BaseClassName::GetStaticTypeName(); } \
		virtual bool IsKindOf(const FName& InTypeName) const { return InTypeName == BaseClassName::GetStaticTypeName(); } \
		template<typename Type> bool IsA() const { return IsKindOf(Type::GetStaticTypeName()); } \
		template<typename Type> const Type& As() const { return *static_cast<const Type*>(this); } \
		template<typename Type> Type& As() { return *static_cast<Type*>(this); } \
	private: \
		static const FName TypeName;

#define MOTIONTIMELINE_DECLARE_TRACK(ClassName, BaseClassName) \
	public: \
		static const FName& GetStaticTypeName() { return ClassName::TypeName; } \
		virtual const FName& GetTypeName() const override { return ClassName::GetStaticTypeName(); } \
		virtual bool IsKindOf(const FName& InTypeName) const override { return InTypeName == ClassName::GetStaticTypeName() || BaseClassName::IsKindOf(InTypeName); } \
	private: \
		static const FName TypeName;


#define MOTIONTIMELINE_IMPLEMENT_TRACK(ClassName) \
	const FName ClassName::TypeName = TEXT(#ClassName);

class FMotionTimelineTrack : public TSharedFromThis<FMotionTimelineTrack>
{
	MOTIONTIMELINE_DECLARE_BASE_TRACK(FMotionTimelineTrack);

public:
	static const float OutlinerRightPadding;

protected:
	TArray<TSharedRef<FMotionTimelineTrack>> Children;
	TWeakPtr<FMotionModel> WeakModel;
	FText DisplayName;
	FText ToolTipText;
	FMotionTrackPadding Padding;
	float Height;
	bool bIsHovered : 1;
	bool bIsVisible : 1;
	bool bIsExpanded : 1;
	bool bIsHeaderTrack : 1;

public:
	FMotionTimelineTrack(const FText& InDisplayName, const FText& InToolTipText, const TSharedPtr<FMotionModel>& InModel, bool bInIsHeaderTrack = false)
		: WeakModel(InModel),
		DisplayName(InDisplayName),
		ToolTipText(InToolTipText),
		Padding(0.0f),
		Height(24.0f),
		bIsHovered(false),
		bIsVisible(true),
		bIsExpanded(true),
		bIsHeaderTrack(bInIsHeaderTrack)
	{
	}

	virtual ~FMotionTimelineTrack() {}

	const TArray<TSharedRef<FMotionTimelineTrack>>& GetChildren() { return Children; }
	void AddChild(const TSharedRef<FMotionTimelineTrack>& InChild) { Children.Add(InChild); }
	void ClearChildren() { Children.Empty(); }
	
	virtual FText GetLabel() const;
	virtual FText GetTooltipText() const;

	TSharedRef<FMotionModel> GetModel() const { return WeakModel.Pin().ToSharedRef(); }

	bool Traverse_ChildFirst(const TFunctionRef<bool(FMotionTimelineTrack&)>& InPredicate, bool bIncludeThisTrack = true);
	bool Traverse_ParentFirst(const TFunctionRef<bool(FMotionTimelineTrack&)>& InPredicate, bool bIncludeThisTrack = true);
	bool TraverseVisible_ChildFirst(const TFunctionRef<bool(FMotionTimelineTrack&)>& InPredicate, bool bIncludeThisTrack = true);
	bool TraverseVisible_ParentFirst(const TFunctionRef<bool(FMotionTimelineTrack&)>& InPredicate, bool bIncludeThisTrack = true);

	virtual TSharedRef<SWidget> GenerateContainerWidgetForOutliner(const TSharedRef<SMotionOutlinerItem>& InRow);
	virtual TSharedRef<SWidget> GenerateContainerWidgetForTimeline();

	virtual void AddToContextMenu(FMenuBuilder& InMenuBuilder, TSet<FName>& InOutExistingMenuTypes) const;

	float GetHeight() const { return Height; }
	void SetHeight(float InHeight) { Height = InHeight; }
	const FMotionTrackPadding& GetPadding() const { return Padding; }
	bool IsHovered() const { return bIsHovered; }
	void SetHovered(bool bInIsHovered) { bIsHovered = bInIsHovered; }
	bool IsVisible() const { return bIsVisible; }
	void SetVisible(bool bInIsVisible) { bIsVisible = bInIsVisible; }
	bool IsExpanded() const { return bIsExpanded; }
	void SetExpanded(bool bInIsExpanded) { bIsExpanded = bInIsExpanded; }
	virtual bool SupportsSelection() const { return false; }
	virtual bool SupportsFiltering() const { return true; }
	virtual bool CanRename() const { return false; }
	virtual void RequestRename() {}

protected:
	TSharedRef<SWidget> GenerateStandardOutlinerWidget(const TSharedRef<SMotionOutlinerItem>& InRow, 
		bool bWithLabelText, TSharedPtr<SBorder>& OutOuterBorder, TSharedPtr<SHorizontalBox>& OutInnerHorizontalBox);

	float GetMinInput() const { return 0.0f; }
	float GetMaxInput() const;
	float GetViewMinInput() const;
	float GetViewMaxInput() const;
	float GetScrubValue() const;
	void SelectObjects(const TArray<UObject*>& SelectedItems);
	void OnSetInputViewRange(float ViewMin, float ViewMax);
};