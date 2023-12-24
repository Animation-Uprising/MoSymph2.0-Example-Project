//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "SMotionBrowser.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "MotionPreProcessToolkit.h"
#include "Animation/BlendSpace1D.h"
#include "MotionSymphonyEditorConstants.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "MotionSymphonyEditor"

void SMotionBrowser::Construct(const FArguments& InArgs,
	TSharedPtr<FMotionPreProcessToolkit> InMotionPreProcessTookitPtr)
{
	MotionPreProcessToolkitPtr = InMotionPreProcessTookitPtr;
	SkeletonName = MotionPreProcessToolkitPtr.Get()->GetSkeletonName();

	TSharedPtr<SVerticalBox> MainBox = SNew(SVerticalBox);
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	AssetPickerConfig.Filter.ClassPaths.Add(UBlendSpace::StaticClass()->GetClassPathName());
	AssetPickerConfig.Filter.ClassPaths.Add(UBlendSpace1D::StaticClass()->GetClassPathName());
	AssetPickerConfig.Filter.ClassPaths.Add(UAnimComposite::StaticClass()->GetClassPathName());
	AssetPickerConfig.SelectionMode = ESelectionMode::Multi;
	AssetPickerConfig.GetCurrentSelectionDelegates.Add(&GetCurrentSelectionDelegate);
	AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SMotionBrowser::FilterAnim);
	AssetPickerConfig.bAllowNullSelection = true;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
	AssetPickerConfig.ThumbnailScale = MotionSymphonyEditorConstants::ThumbnailSize;

	AssetPickerConfig.Filter.bRecursiveClasses = true;
	AssetPickerConfig.bAddFilterUI = true;
	AssetPickerConfig.bShowPathInColumnView = true;
	AssetPickerConfig.bSortByPathInColumnView = true;
	AssetPickerConfig.bAllowDragging = true;

	// hide all asset registry columns by default (we only really want the name and path)
	TArray<UObject::FAssetRegistryTag> AssetRegistryTags;
	UAnimSequence::StaticClass()->GetDefaultObject()->GetAssetRegistryTags(AssetRegistryTags);
	for (UObject::FAssetRegistryTag& AssetRegistryTag : AssetRegistryTags)
	{
		AssetPickerConfig.HiddenColumnNames.Add(AssetRegistryTag.Name.ToString());
	}

	// Also hide the type column by default (but allow users to enable it, so don't use bShowTypeInColumnView)
	AssetPickerConfig.HiddenColumnNames.Add(TEXT("Class"));
	
	
	MainBox->AddSlot()
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];

	ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
			.Padding(FMargin(1.0f, 1.0f, 1.0f, 1.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[

					SNew(SScrollBox)
					+ SScrollBox::Slot()
					.Padding(1.0f)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						MainBox.ToSharedRef()
					]

				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SUniformGridPanel)
						.SlotPadding(1)
						+ SUniformGridPanel::Slot(0, 0)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
							.ForegroundColor(FLinearColor::White)
							.Text(LOCTEXT("AddButton", "Add"))
							.HAlign(HAlign_Fill)
							.OnClicked(this, &SMotionBrowser::AddClicked)
						]
					]
				]
			]
		];
}

SMotionBrowser::~SMotionBrowser()
{
}

bool SMotionBrowser::FilterAnim(const FAssetData& AssetData) const
{
	if(AssetData.GetClass()->IsChildOf(UAnimationAsset::StaticClass()))
	{
		if(const USkeleton* DesiredSkeleton = MotionPreProcessToolkitPtr->GetSkeleton())
		{
			const UAnimationAsset* AnimAsset = Cast<UAnimationAsset>(AssetData.GetAsset());
			return !DesiredSkeleton->IsCompatibleForEditor(AnimAsset->GetSkeleton());
		}
	}

	return true;
}

FReply SMotionBrowser::AddClicked() const
{
	TArray<FAssetData> SelectionArray = GetCurrentSelectionDelegate.Execute();

	if (SelectionArray.Num() > 0)
	{
		TArray<UAnimSequence*> StoredSequences;
		TArray<UBlendSpace*> StoredBlendSpaces;
		TArray<UAnimComposite*> StoredComposites;

		for (int i = 0; i < SelectionArray.Num(); ++i)
		{
			if(!SelectionArray[i].IsAssetLoaded())
			{
				SelectionArray[i].GetPackage()->FullyLoad();
			}
			
			if (SelectionArray[i].IsAssetLoaded())
			{
				UAnimSequence* NewSequence = Cast<UAnimSequence>(SelectionArray[i].GetAsset());
				UBlendSpace* NewBlendSpace = Cast<UBlendSpace>(SelectionArray[i].GetAsset());
				UAnimComposite* NewComposite = Cast<UAnimComposite>(SelectionArray[i].GetAsset());
				if (NewSequence)
				{
					StoredSequences.Add(NewSequence);
				}
				else if (NewBlendSpace)
				{
					StoredBlendSpaces.Add(NewBlendSpace);
				}
				else if (NewComposite)
				{
					StoredComposites.Add(NewComposite);
				}
			}
		}

		if (StoredSequences.Num() > 0)
		{
			MotionPreProcessToolkitPtr.Get()->AddNewAnimSequences(StoredSequences);
		}

		if (StoredBlendSpaces.Num() > 0)
		{
			MotionPreProcessToolkitPtr.Get()->AddNewBlendSpaces(StoredBlendSpaces);
		}

		if (StoredComposites.Num() > 0)
		{
			MotionPreProcessToolkitPtr.Get()->AddNewComposites(StoredComposites);
		}
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoAnimationsSelected", "No Animations Selected."));
	}

	return FReply::Handled();
}



