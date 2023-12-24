//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Enumerations/EMotionMatchingEnums.h"
#include "Input/Reply.h"
#include "ITransportControl.h"

class UDebugSkelMeshComponent;
class UAnimSequenceBase;
class UAnimSingleNodeInstance;

class SMotionTimelineTransportControls : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMotionTimelineTransportControls) {}

	SLATE_END_ARGS()

private:
	UDebugSkelMeshComponent* DebugSkelMesh;
	UAnimationAsset* AnimAsset;
	EMotionAnimAssetType AnimType;

	float FrameRate;
	float PlayLength;

public:
	void Construct(const FArguments& InArgs, UDebugSkelMeshComponent* InDebugMesh, 
		UAnimationAsset* InAnimAsset, EMotionAnimAssetType InAnimType);

private:
	UAnimSingleNodeInstance* GetPreviewInstance() const;

	//TSharedRef<IPersonaPreviewScene> GetPreviewScene() const { return WeakPreviewScene.Pin().ToSharedRef(); }

	FReply OnClick_Forward_Step();
	FReply OnClick_Forward_End();
	FReply OnClick_Backward_Step();
	FReply OnClick_Backward_End();
	FReply OnClick_Forward();
	FReply OnClick_Backward();
	FReply OnClick_ToggleLoop();
	FReply OnClick_Record();

	bool IsLoopStatusOn() const;

	EPlaybackMode::Type GetPlaybackMode() const;

	bool IsRecording() const;
};