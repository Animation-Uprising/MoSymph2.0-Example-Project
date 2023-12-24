//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"
#include "ContentBrowserDelegates.h"

class SMotionBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMotionBrowser)
	{}
	SLATE_END_ARGS()

private:
	TSharedPtr<class FMotionPreProcessToolkit> MotionPreProcessToolkitPtr;
	
public:
	void Construct(const FArguments& InArgs, TSharedPtr<class FMotionPreProcessToolkit> InMotionPreProcessTookitPtr);
	virtual ~SMotionBrowser() override;
	bool FilterAnim(const FAssetData& AssetData) const;

private:
	FGetCurrentSelectionDelegate GetCurrentSelectionDelegate;
	FString SkeletonName;
	FReply AddClicked() const;
};