//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Engine/Texture2D.h"
#include "IDetailsView.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "IDetailCustomization.h"
#include "ContentBrowserDelegates.h"

struct FAssetData;
class IDetailLayoutBuilder;
class IPropertyHandle;
class SComboButton;
class UAnimSequence;
class FAssetThumbnailPool;

class SAddNewAnimDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAddNewAnimDialog) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class FMotionPreProcessToolkit> InMotionPreProcessTookitPtr);

	~SAddNewAnimDialog();

	static bool ShowWindow(TSharedPtr<FMotionPreProcessToolkit> InMotionPreProcessToolkitPtr);

	bool FilterAnim(const FAssetData& AssetData);

private:
	FGetCurrentSelectionDelegate GetCurrentSelectionDelegate;
	FString SkeletonName;
	FReply AddClicked();
	FReply CancelClicked();
	void CloseContainingWindow();
	TSharedPtr<class IDetailsView> MainPropertyView;
	TSharedPtr<class FMotionPreProcessToolkit> MotionPreProcessToolkitPtr;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
};