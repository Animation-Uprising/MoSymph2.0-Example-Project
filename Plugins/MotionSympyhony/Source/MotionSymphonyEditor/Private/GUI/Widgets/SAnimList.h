//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Enumerations/EMotionMatchingEnums.h"

class UAnimComposite;
class SBorder;
class SScrollBox;
class SBox;
class SButton;
class STextBlock;
class FMotionPreProcessToolkit;

/*Contains the data for a single node in the motion anim tree view */
class FAnimTreeItem
{
private:
	/** Parent item or NULL if this is a root*/
	TWeakPtr<FAnimTreeItem> ParentDir;

	/** Full path of this directory in the tree */
	FString DirectoryPath;

	/** Display name of the category */
	FString DisplayName;

	/** Child categories */
	TArray<TSharedPtr<FAnimTreeItem>> SubDirectories;

	int32 AnimId;
	EMotionAnimAssetType AnimType;
	
public:
	/* Returns the parent directory or nullptr if this is a root node*/
	TSharedPtr<FAnimTreeItem> GetParentCategory() const
	{
		return ParentDir.Pin();
	}

	const FString& GetDisplayName() const
	{
		return DisplayName;
	}

	/* Returns the path of this item, read-only */
	FString GetDirectoryPath() const
	{
		return DirectoryPath;
	}

	/* Returns the path of this item, read or write */
	const TArray<TSharedPtr<FAnimTreeItem>>& GetSubDirectories() const
	{
		return SubDirectories;
	}

	/* Returns all subdirectories, read or write */
	TArray<TSharedPtr<FAnimTreeItem>>& AccessSubDirectories()
	{
		return SubDirectories;
	}

	/* Returns a subdirectory to this node in the tree */
	void AddSubDirectory(const TSharedPtr<FAnimTreeItem> NewSubDir)
	{
		SubDirectories.Add(NewSubDir);
	}

	int32 GetAnimId() const
	{
		return AnimId;
	}

	EMotionAnimAssetType GetAnimType() const
	{
		return AnimType;
	}

public:
	FAnimTreeItem(const TSharedPtr<FAnimTreeItem> InParentDir, const FString& InDirectoryPath,
			const FString& InDisplayName, int32 InAnimId = -1, EMotionAnimAssetType InAnimType = EMotionAnimAssetType::None)
		: ParentDir(InParentDir),
		DirectoryPath(InDirectoryPath),
		DisplayName(InDisplayName),
		AnimId(InAnimId),
		AnimType(InAnimType)
	{
	}
};

typedef STreeView<TSharedPtr<FAnimTreeItem>> SAnimTreeView;

class SAnimTree : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAnimTree)
	{}
	SLATE_END_ARGS()

public:
	TWeakPtr<class FMotionPreProcessToolkit> MotionPreProcessToolkitPtr;

	TSharedPtr<FAnimTreeItem> SequenceDirectory;
	TSharedPtr<FAnimTreeItem> CompositeDirectory;
	TSharedPtr<FAnimTreeItem> BlendSpaceDirectory;

private:
	/** The tree view widget*/
	TSharedPtr<SAnimTreeView> AnimTreeView;

	/** The Core Data for the tree viewer */
	TArray<TSharedPtr<FAnimTreeItem>> Directories;

public:
	void Construct(const FArguments& InArgs, TWeakPtr<class FMotionPreProcessToolkit> InMotionPreProcessToolkit);

	virtual ~SAnimTree() override;

	TSharedPtr<FAnimTreeItem> GetSelectedDirectory() const;
	void SelectDirectory(const TSharedPtr<FAnimTreeItem>& CategoryToSelect);
	bool IsItemExpanded(const TSharedPtr<FAnimTreeItem> Item) const;

	//List Management
	void AddAnimSequence(UAnimSequence* AnimSequence, uint32 InAnimId);
	void AddAnimComposite(UAnimComposite* AnimComposite, uint32 InAnimId);
	void AddBlendSpace(UBlendSpace* BlendSpace, uint32 InAnimId);

	void RemoveAnimSequence(int32 AnimId) const;
	void RemoveAnimComposite(int32 AnimId) const;
	void RemoveBlendSpace(int32 AnimId) const;
	void RemoveAnimFromDirectory(int32 AnimId, TArray<TSharedPtr<FAnimTreeItem>>& DirectoryItems) const;

	/** Rebuilds the category tree from scratch */
	void RebuildAnimTree();

private:
	/** Called to generate a widget for the specified tree item*/
	TSharedRef<ITableRow> AnimTree_OnGenerateRow(TSharedPtr<FAnimTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Given a tree item, fills an array of child items */
	void AnimTree_OnGetChildren(TSharedPtr<FAnimTreeItem> Item, TArray<TSharedPtr<FAnimTreeItem>>& OutChildren);

	/** Called when the user clicks on an item, or when selection changes by some other means */
	void AnimTree_OnSelectionChanged(TSharedPtr<FAnimTreeItem> Item, ESelectInfo::Type SelectInfo);

	void AnimTree_OnDeleteSelectedAnim();
	void AnimTree_OnCopySelectedAnims() const;
	void AnimTree_OnPasteAnims();

	TSharedPtr<SWidget> AnimTree_OnContextMenuOpening();
};