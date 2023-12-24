//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "SEditorViewport.h"
#include "EditorViewportClient.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "MotionPreProcessToolkit.h"
#include "MotionPreProcessToolkitViewportClient.h"
#include "MotionPreProcessToolkitViewportToolbar.h"

class SMotionPreProcessToolkitViewport : public SEditorViewport, public ICommonEditorViewportToolbarInfoProvider
{

public:
	SLATE_BEGIN_ARGS(SMotionPreProcessToolkitViewport)
		: _MotionDataBeingEdited((UMotionDataAsset*)nullptr)
	{}

	SLATE_ATTRIBUTE(UMotionDataAsset*, MotionDataBeingEdited)
	SLATE_END_ARGS()

private:
	TAttribute<UMotionDataAsset*> MotionDataBeingEdited;
	TWeakPtr<class FMotionPreProcessToolkit> MotionPreProcessToolkitPtr;
	TSharedPtr<class FMotionPreProcessToolkitViewportClient> EditorViewportClient;
	
public:
	void Construct(const FArguments& InArgs, TWeakPtr<class FMotionPreProcessToolkit> InMotionPreProcessToolkitPtr);

	void SetupAnimatedRenderComponent();

	// SEditorViewport interface
	virtual void BindCommands() override;
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual EVisibility GetTransformToolbarVisibility() const override;
	virtual void OnFocusViewportToSelection() override;
	//End of SEditorViewport interface

	// ICommonEditorViewportToolbarInfoProvider interface
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	// End of ICommonEditorViewportTOolbarInfoProvider interface

	UDebugSkelMeshComponent* GetPreviewComponent() const;

	bool IsMirror() const;
};