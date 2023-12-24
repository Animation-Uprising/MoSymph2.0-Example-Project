//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "SAnimList.h"

#include "IContentBrowserSingleton.h"
#include "MotionPreProcessorToolkitCommands.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyCustomizationHelpers.h"
#include "MotionPreProcessToolkit.h"
#include "Animation/BlendSpace.h"
#include "Objects/MotionAnimObject.h"


#define LOCTEXT_NAMESPACE "AnimList"

void SAnimTree::Construct(const FArguments& InArgs, TWeakPtr<class FMotionPreProcessToolkit> InMotionPreProcessToolkit)
{
	MotionPreProcessToolkitPtr = InMotionPreProcessToolkit;
	
	AnimTreeView =
		SNew(SAnimTreeView)
		.SelectionMode(ESelectionMode::Multi)
		.ClearSelectionOnClick(false)
		.TreeItemsSource(&Directories)
		.OnGenerateRow(this, &SAnimTree::AnimTree_OnGenerateRow)
		.OnGetChildren(this, &SAnimTree::AnimTree_OnGetChildren)
		.OnSelectionChanged(this, &SAnimTree::AnimTree_OnSelectionChanged)
		.OnContextMenuOpening(this, &SAnimTree::AnimTree_OnContextMenuOpening)
	;

	ChildSlot.AttachWidget(AnimTreeView.ToSharedRef());

	RebuildAnimTree();
}

SAnimTree::~SAnimTree()
{
}

TSharedPtr<FAnimTreeItem> SAnimTree::GetSelectedDirectory() const
{
	if(AnimTreeView.IsValid())
	{
		auto SelectedItems = AnimTreeView->GetSelectedItems();
		if(SelectedItems.Num() > 0)
		{
			const auto& SelectedCategoryItem = SelectedItems[0];
			return SelectedCategoryItem;
		}
	}

	return nullptr;
}

void SAnimTree::SelectDirectory(const TSharedPtr<FAnimTreeItem>& CategoryToSelect)
{
	if(ensure(AnimTreeView.IsValid()))
	{
		AnimTreeView->SetSelection(CategoryToSelect);
	}
}

bool SAnimTree::IsItemExpanded(const TSharedPtr<FAnimTreeItem> Item) const
{
	return AnimTreeView->IsItemExpanded(Item);
}

void SAnimTree::AddAnimSequence(UAnimSequence* AnimSequence, uint32 InAnimId)
{
	TSharedPtr<FAnimTreeItem> SequenceItem = MakeShareable(new FAnimTreeItem(SequenceDirectory,
				AnimSequence->GetName(), AnimSequence->GetName(),
				InAnimId, EMotionAnimAssetType::Sequence));
	SequenceDirectory->AddSubDirectory(SequenceItem);

	//Refresh
	if(AnimTreeView.IsValid())
	{
		AnimTreeView->RequestTreeRefresh();
	}
}

void SAnimTree::AddAnimComposite(UAnimComposite* AnimComposite, uint32 InAnimId)
{
	TSharedPtr<FAnimTreeItem> CompositeItem = MakeShareable(new FAnimTreeItem(CompositeDirectory,
				AnimComposite->GetName(), AnimComposite->GetName(),
				InAnimId, EMotionAnimAssetType::Composite));
	CompositeDirectory->AddSubDirectory(CompositeItem);

	//Refresh
	if(AnimTreeView.IsValid())
	{
		AnimTreeView->RequestTreeRefresh();
	}
}

void SAnimTree::AddBlendSpace(UBlendSpace* BlendSpace, uint32 InAnimId)
{
	TSharedPtr<FAnimTreeItem> BlendSpaceItem = MakeShareable(new FAnimTreeItem(BlendSpaceDirectory,
					BlendSpace->GetName(), BlendSpace->GetName(),
					InAnimId, EMotionAnimAssetType::BlendSpace));
	BlendSpaceDirectory->AddSubDirectory(BlendSpaceItem);

	//Refresh
	if(AnimTreeView.IsValid())
	{
		AnimTreeView->RequestTreeRefresh();
	}
}

void SAnimTree::RemoveAnimSequence(int32 AnimId) const
{
	RemoveAnimFromDirectory(AnimId, SequenceDirectory->AccessSubDirectories());
}

void SAnimTree::RemoveAnimComposite(int32 AnimId) const
{
	RemoveAnimFromDirectory(AnimId, CompositeDirectory->AccessSubDirectories());
}

void SAnimTree::RemoveBlendSpace(int32 AnimId) const
{
	RemoveAnimFromDirectory(AnimId, BlendSpaceDirectory->AccessSubDirectories());
}

void SAnimTree::RemoveAnimFromDirectory(int32 AnimId, TArray<TSharedPtr<FAnimTreeItem>>& DirectoryItems) const
{
	if(AnimId < 0 || AnimId >= DirectoryItems.Num())
	{
		return;
	}

	for(int32 i = 0; i < DirectoryItems.Num(); ++i)
	{
		TSharedPtr<FAnimTreeItem> AnimTreeItem = DirectoryItems[i];

		if(AnimTreeItem.IsValid() && AnimTreeItem->GetAnimId() == AnimId)
		{
			DirectoryItems.RemoveAt(i);
			AnimTreeView->RequestTreeRefresh();
			break;
		}
	}
}

TSharedRef<ITableRow> SAnimTree::AnimTree_OnGenerateRow(TSharedPtr<FAnimTreeItem> Item,
                                                        const TSharedRef<STableViewBase>& OwnerTable)
{
	if(!Item.IsValid())
	{
		return SNew(STableRow<TSharedPtr<FAnimTreeItem>>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NullLabel", "THIS WAS NULL SOMEHOW"))
		];
	}

	FSlateFontInfo FontInfo = FTextBlockStyle::GetDefault().Font;
	FontInfo.Size = !Item->GetParentCategory() ? 13 : 10;

	return SNew(STableRow<TSharedPtr<FAnimTreeItem>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Item->GetDisplayName()))
		.Font(FontInfo)
	];
}

void SAnimTree::AnimTree_OnGetChildren(TSharedPtr<FAnimTreeItem> Item, TArray<TSharedPtr<FAnimTreeItem>>& OutChildren)
{
	const auto& SubCategories = Item->GetSubDirectories();
	OutChildren.Append(SubCategories);
}

void SAnimTree::AnimTree_OnSelectionChanged(TSharedPtr<FAnimTreeItem> Item, ESelectInfo::Type SelectInfo)
{
	if(!Item.IsValid() || !MotionPreProcessToolkitPtr.IsValid())
	{
		return;
	}
	
	MotionPreProcessToolkitPtr.Pin().Get()->SetCurrentAnimation(Item->GetAnimId(), Item->GetAnimType());
	MotionPreProcessToolkitPtr.Pin().Get()->SetAnimationSelection(AnimTreeView->GetSelectedItems());
}

void SAnimTree::AnimTree_OnDeleteSelectedAnim()
{
	FAnimTreeItem* AnimTreeItem = GetSelectedDirectory().Get();

	if(!AnimTreeItem)
	{
		return;
	}
	
	switch(AnimTreeItem->GetAnimType())
	{
		case EMotionAnimAssetType::Sequence:
			MotionPreProcessToolkitPtr.Pin().Get()->DeleteAnimSequence(AnimTreeItem->GetAnimId());	
		break;
		case EMotionAnimAssetType::Composite:
			MotionPreProcessToolkitPtr.Pin().Get()->DeleteComposite(AnimTreeItem->GetAnimId());	
		break;
		case EMotionAnimAssetType::BlendSpace:
			MotionPreProcessToolkitPtr.Pin().Get()->DeleteBlendSpace(AnimTreeItem->GetAnimId());
		break;
		default:
		break;
	}
}

void SAnimTree::AnimTree_OnCopySelectedAnims() const
{
	if(AnimTreeView.IsValid())
	{
		auto SelectedItems = AnimTreeView->GetSelectedItems();

		MotionPreProcessToolkitPtr.Pin()->ClearCopyClipboard();
		for(const auto Item : SelectedItems)
		{
			MotionPreProcessToolkitPtr.Pin()->CopyAnim(Item->GetAnimId(), Item->GetAnimType());
		}
	}
}

void SAnimTree::AnimTree_OnPasteAnims()
{
	MotionPreProcessToolkitPtr.Pin()->PasteAnims();
	
	RebuildAnimTree();
}

TSharedPtr<SWidget> SAnimTree::AnimTree_OnContextMenuOpening()
{
	const TSharedPtr<FUICommandList>  Commands;
	
	FMenuBuilder MenuBuilder(true, Commands);
	MenuBuilder.BeginSection("Animation");
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AnimMenuDeleteAnim", "Delete"),
		LOCTEXT("AnimMenuDeleteAnim_Tooltip", "Delete's the currently selected animation."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SAnimTree::AnimTree_OnDeleteSelectedAnim))
		);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AnimMenuCopyAnim", "Copy"),
		LOCTEXT("AnimMenuCopyAnim_Tooltip", "Copy this animation entry to the clip board."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SAnimTree::AnimTree_OnCopySelectedAnims))
		);
	
	if(MotionPreProcessToolkitPtr.Pin()->HasCopiedAnims())
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AnimMenuPasteAnim", "Paste"),
			LOCTEXT("AnimMenuPasteAnim_Tooltip", "Past any animations from the clipboard into this anim data."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SAnimTree::AnimTree_OnPasteAnims))
			);
	}
	
	MenuBuilder.EndSection();
	
	return MenuBuilder.MakeWidget();
}

void SAnimTree::RebuildAnimTree()
{
	/** Workaround for tree un-expanding when rebuilt*/
	const bool bSequenceDirExpanded = IsItemExpanded(SequenceDirectory);
	const bool bCompositeDirExpanded = IsItemExpanded(CompositeDirectory);
	const bool bBlendSpaceDirExpanded = IsItemExpanded(BlendSpaceDirectory);

	Directories.Empty();

	//~~~~~~~~~~~~~~~~~~~
	//Root Level
	SequenceDirectory = MakeShareable(new FAnimTreeItem(nullptr,
		TEXT("SequenceDirectory"), FString("Sequences")));
	Directories.Add(SequenceDirectory);

	CompositeDirectory = MakeShareable(new FAnimTreeItem(nullptr,
		TEXT("CompositeDirectory"), FString("Composites")));
	Directories.Add(CompositeDirectory);

	BlendSpaceDirectory = MakeShareable(new FAnimTreeItem(nullptr,
		TEXT("BlendSpaceDirectory"), FString("BlendSpaces")));
	Directories.Add(BlendSpaceDirectory);
	//~~~~~~~~~~~~~~~~~~~

	UMotionDataAsset* MotionData = MotionPreProcessToolkitPtr.Pin()->GetActiveMotionDataAsset();
	if(MotionData)
	{
		
		for(TObjectPtr<UMotionSequenceObject> MotionSequence : MotionData->SourceMotionSequenceObjects)
		{
			if(MotionSequence && MotionSequence->Sequence)
			{
				TSharedPtr<FAnimTreeItem> SequenceItem = MakeShareable(new FAnimTreeItem(SequenceDirectory,
					MotionSequence->Sequence->GetName(), MotionSequence->Sequence->GetName(),
					MotionSequence->AnimId, EMotionAnimAssetType::Sequence));
				SequenceDirectory->AddSubDirectory(SequenceItem);
			}
		}

		for(TObjectPtr<UMotionCompositeObject> MotionComposite : MotionData->SourceCompositeObjects)
		{
			if(MotionComposite && MotionComposite->AnimComposite)
			{
				TSharedPtr<FAnimTreeItem> CompositeItem = MakeShareable(new FAnimTreeItem(CompositeDirectory,
				MotionComposite->AnimComposite->GetName(), MotionComposite->AnimComposite->GetName(),
				MotionComposite->AnimId, EMotionAnimAssetType::Composite));
				CompositeDirectory->AddSubDirectory(CompositeItem);
			}
		}

		for(TObjectPtr<UMotionBlendSpaceObject> MotionBlendSpace : MotionData->SourceBlendSpaceObjects)
		{
			if(MotionBlendSpace && MotionBlendSpace->BlendSpace)
			{
				TSharedPtr<FAnimTreeItem> BlendSpaceItem = MakeShareable(new FAnimTreeItem(BlendSpaceDirectory,
				MotionBlendSpace->BlendSpace->GetName(), MotionBlendSpace->BlendSpace->GetName(),
				MotionBlendSpace->AnimId, EMotionAnimAssetType::BlendSpace));
				BlendSpaceDirectory->AddSubDirectory(BlendSpaceItem);
			}
		}
	}

	//Refresh
	if(AnimTreeView.IsValid())
	{
		AnimTreeView->RequestTreeRefresh();
		AnimTreeView->SetItemExpansion(SequenceDirectory, bSequenceDirExpanded);
		AnimTreeView->SetItemExpansion(CompositeDirectory, bCompositeDirExpanded);
		AnimTreeView->SetItemExpansion(BlendSpaceDirectory, bBlendSpaceDirExpanded);
	}
}

#undef LOCTEXT_NAMESPACE

