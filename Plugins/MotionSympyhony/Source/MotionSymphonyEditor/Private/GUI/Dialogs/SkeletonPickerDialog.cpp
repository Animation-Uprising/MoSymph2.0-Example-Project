//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "SkeletonPickerDialog.h"
#include "MotionPreProcessToolkit.h"

#include "CoreMinimal.h"
#include "ContentBrowserModule.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"

#include "IContentBrowserSingleton.h"

#define LOCTEXT_NAMESPACE "MotionSymphonyEditor"

FFollowUpWindowDelegate SSkeletonPickerDialog::OnOpenFollowUpWindow = FFollowUpWindowDelegate();

void SSkeletonPickerDialog::Construct(const FArguments& InArgs, TSharedPtr<FMotionPreProcessToolkit> InMotionPreProcessToolkitPtr)
{
	MotionPreProcessToolkitPtr = InMotionPreProcessToolkitPtr;

	TSharedPtr<SVerticalBox> MainBox = SNew(SVerticalBox);

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassPaths.Add(USkeleton::StaticClass()->GetClassPathName());
	AssetPickerConfig.SelectionMode = ESelectionMode::Single;
	AssetPickerConfig.GetCurrentSelectionDelegates.Add(&GetCurrentSelectionDelegate);
	AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SSkeletonPickerDialog::FilterSkeletons);
	AssetPickerConfig.bAllowNullSelection = true;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
	AssetPickerConfig.ThumbnailScale = 64.0f;
	
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
							.Text(LOCTEXT("SetButton", "Set"))
							.OnClicked(this, &SSkeletonPickerDialog::AddClicked)
						]
						+ SUniformGridPanel::Slot(1, 0)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "FlatButton")
							.ForegroundColor(FLinearColor::White)
							.Text(LOCTEXT("CancelButton", "Cancel"))
							.OnClicked(this, &SSkeletonPickerDialog::CancelClicked)
						]
					]
				]
			]
		];
}

SSkeletonPickerDialog::~SSkeletonPickerDialog()
{

}

bool SSkeletonPickerDialog::ShowWindow(TSharedPtr<FMotionPreProcessToolkit> InMotionPreProcessToolkit)
{
	const FText TitleText = NSLOCTEXT("MotionPreProcessToolkit", "MotionPreProcessToolkit_PickSkeleton", "Pick Skeleton");
	TSharedRef<SWindow> AddNewAnimatoinsWindow = SNew(SWindow)
		.Title(TitleText)
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(550, 550.f))
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SupportsMinimize(false);

	TSharedRef<SSkeletonPickerDialog> AddNewAnimsDialog = SNew(SSkeletonPickerDialog, InMotionPreProcessToolkit);

	AddNewAnimatoinsWindow->SetContent(AddNewAnimsDialog);
	TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
	if (RootWindow.IsValid())
	{
		FSlateApplication::Get().AddWindowAsNativeChild(AddNewAnimatoinsWindow, RootWindow.ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(AddNewAnimatoinsWindow);
	}

	return false;
}

bool SSkeletonPickerDialog::FilterSkeletons(const FAssetData& AssetData)
{
	if (!AssetData.IsAssetLoaded())
	{
		AssetData.GetPackage()->FullyLoad();
	}

	return false;
}

FReply SSkeletonPickerDialog::AddClicked()
{
	TArray<FAssetData> SelectionArray = GetCurrentSelectionDelegate.Execute();

	if (SelectionArray.Num() == 1)
	{
		FAssetData& selectedAsset = SelectionArray[0];

		if (selectedAsset.IsAssetLoaded())
		{
			USkeleton* selectedSkeleton = Cast<USkeleton>(selectedAsset.GetAsset());
			if (selectedSkeleton)
			{
				MotionPreProcessToolkitPtr.Get()->SetSkeleton(selectedSkeleton);
			}
		}

		OnOpenFollowUpWindow.ExecuteIfBound();
		SSkeletonPickerDialog::OnOpenFollowUpWindow.Unbind();

		CloseContainingWindow();
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("No Skeleton Selected", "No Skeleton Selected."));
	}

	return FReply::Handled();
}

FReply SSkeletonPickerDialog::CancelClicked()
{
	SSkeletonPickerDialog::OnOpenFollowUpWindow.Unbind();
	CloseContainingWindow();
	return FReply::Handled();
}

void SSkeletonPickerDialog::CloseContainingWindow()
{
	TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (ContainingWindow.IsValid())
	{
		ContainingWindow->RequestDestroyWindow();
	}
}

#undef LOCTEXT_NAMESPACE