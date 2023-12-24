//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MotionSymphonyEditor.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Engine/Texture2D.h"
#include "IDetailsView.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "IDetailCustomization.h"
#include "ContentBrowserDelegates.h"
//#include "DelegateCombinations.h"

struct FAssetData;
class IDetailLayoutBuilder;
class IPropertyHandle;
class SComboButton;
class FAssetThumbnailPool;

DECLARE_DELEGATE(FFollowUpWindowDelegate)

class SSkeletonPickerDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSkeletonPickerDialog) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class FMotionPreProcessToolkit> InMotionPreProcessTookitPtr);

	~SSkeletonPickerDialog();

	static bool ShowWindow(TSharedPtr<FMotionPreProcessToolkit> InMotionPreProcessToolkitPtr);

	bool FilterSkeletons(const FAssetData& AssetData);

	static FFollowUpWindowDelegate OnOpenFollowUpWindow;

private:
	FGetCurrentSelectionDelegate GetCurrentSelectionDelegate;
	FReply AddClicked();
	FReply CancelClicked();
	void CloseContainingWindow();
	TSharedPtr<class IDetailsView> MainPropertyView;
	TSharedPtr<class FMotionPreProcessToolkit> MotionPreProcessToolkitPtr;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
};