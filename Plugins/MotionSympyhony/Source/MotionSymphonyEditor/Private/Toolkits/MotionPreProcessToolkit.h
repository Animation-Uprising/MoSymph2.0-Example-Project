//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "Objects/Assets/MotionDataAsset.h"
#include "MotionPreProcessToolkitViewport.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/GCObject.h"
#include "EditorUndoClient.h"
#include "ITransportControl.h"

class SMotionBrowser;
class FSpawnTabArgs;
class ISlateStyle;
class IToolkitHost;
class IDetailsView;
class SDockTab;
class UMotionDataAsset;
class SAnimTree;
class SMotionTimeline;

class FMotionPreProcessToolkit
	: public FAssetEditorToolkit
	, public FEditorUndoClient
	, public FGCObject
{
public:
	FMotionPreProcessToolkit(){}
	virtual ~FMotionPreProcessToolkit();

#if ENGINE_MINOR_VERSION > 2
	FString GetReferencerName() const override;
#endif

public:
	int32 CurrentAnimIndex;
	EMotionAnimAssetType CurrentAnimType;
	FText CurrentAnimName;

	int32 PreviewPoseStartIndex;
	int32 PreviewPoseEndIndex;
	int32 PreviewPoseCurrentIndex;

	static TArray<TObjectPtr<UMotionSequenceObject>> CopiedSequences;
	static TArray<TObjectPtr<UMotionCompositeObject>> CopiedComposites;
	static TArray<TObjectPtr<UMotionBlendSpaceObject>> CopiedBlendSpaces;
	
protected: 
	UMotionDataAsset* ActiveMotionDataAsset;
	TSharedPtr<SAnimTree> AnimationTreePtr;
	TSharedPtr<SMotionBrowser> MotionBrowserPtr;
	TSharedPtr<class SMotionPreProcessToolkitViewport> ViewportPtr;
	TSharedPtr<class SMotionTimeline> MotionTimelinePtr;

	mutable float ViewInputMin;
	mutable float ViewInputMax;
	mutable float LastObservedSequenceLength;

	TArray<FVector> CachedTrajectoryPoints;

	bool PendingTimelineRebuild = false;

public:
	void Initialize(UMotionDataAsset* InPreProcessAsset, const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost> InToolkitHost );

	virtual FString GetDocumentationLink() const override;

	// IToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	// End of IToolkit interface

	// FAssetEditorToolkit 
	virtual FText GetBaseToolkitName() const override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	// End of FAssetEditorToolkit

	// FSerializableObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FSerializableObject interface

	UMotionDataAsset* GetActiveMotionDataAsset() const;

	FText GetAnimationName(const int32 AnimIndex);
	FText GetBlendSpaceName(const int32 BlendSpaceIndex);
	FText GetCompositeName(const int32 CompositeIndex);
	void SetCurrentAnimation(const int32 AnimIndex, const EMotionAnimAssetType, const bool bForceRefresh=false);
	void SetAnimationSelection(const TArray<TSharedPtr<class FAnimTreeItem>>& SelectedItems);
	//void SetCurrentBlendSpace(const int32 BlendSpaceIndex);
	//void SetCurrentComposite(const int32 CompositeIndex);
	TObjectPtr<UMotionAnimObject> GetCurrentMotionAnim() const;
	UAnimSequence* GetCurrentAnimation() const;
	void DeleteAnimSequence(const int32 AnimIndex);
	void DeleteBlendSpace(const int32 BlendSpaceIndex);
	void DeleteComposite(const int32 CompositeIndex);
	void CopyAnim(const int32 CopyAnimIndex, const EMotionAnimAssetType AnimType) const;
	void PasteAnims();
	static void ClearCopyClipboard();
	void ClearAnimList();
	void ClearBlendSpaceList();
	void ClearCompositeList();
	void AddNewAnimSequences(TArray<UAnimSequence*> FromSequences);
	void AddNewBlendSpaces(TArray<UBlendSpace*> FromBlendSpaces);
	void AddNewComposites(TArray<UAnimComposite*> FromComposites);
	void SelectPreviousAnim();
	void SelectNextAnim();
	void RefreshAnim();
	bool AnimationAlreadyAdded(const FName SequenceName) const;
	FString GetSkeletonName() const;
	USkeleton* GetSkeleton() const;
	void SetSkeleton(USkeleton* Skeleton) const;

	UDebugSkelMeshComponent* GetPreviewSkeletonMeshComponent() const;
	bool SetPreviewComponentSkeletalMesh(USkeletalMesh* SkeletalMesh) const;

	void SetCurrentFrame(int32 NewIndex);
	int32 GetCurrentFrame() const;
	float GetFramesPerSecond() const;

	void FindCurrentPose(float Time);

	void DrawCachedTrajectoryPoints(FPrimitiveDrawInterface* DrawInterface) const;

	bool GetPendingTimelineRebuild();
	void SetPendingTimelineRebuild(const bool IsPendingRebuild);

	void HandleTagsSelected(const TArray<UObject*>& SelectedTags);

	static bool HasCopiedAnims();

protected:
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Animations(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_AnimationDetails(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_MotionBrowser(const FSpawnTabArgs& Args);

	TSharedPtr<class IDetailsView> DetailsView;
	TSharedPtr<class IDetailsView> AnimDetailsView;

	void BindCommands();
	void ExtendMenu();
	void ExtendToolbar();

	//Timeline callbacks
	FReply OnClick_Forward();
	FReply OnClick_Forward_Step();
	FReply OnClick_Forward_End();
	FReply OnClick_Backward();
	FReply OnClick_Backward_Step();
	FReply OnClick_Backward_End();
	FReply OnClick_ToggleLoop();

	uint32 GetTotalFrameCount() const;
	uint32 GetTotalFrameCountPlusOne() const;
	float GetTotalSequenceLength() const;
	float GetPlaybackPosition() const;
	void SetPlaybackPosition(float NewTime);
	bool IsLooping() const;
	EPlaybackMode::Type GetPlaybackMode() const;

	float GetViewRangeMin() const;
	float GetViewRangeMax() const;
	void SetViewRange(float NewMin, float NewMax);

private:
	bool IsValidAnim(const int32 AnimIndex) const;
	bool IsValidAnim(const int32 AnimIndex, const EMotionAnimAssetType AnimType) const;
	bool IsValidBlendSpace(const int32 BlendSpaceIndex) const;
	bool IsValidComposite(const int32 CompositeIndex) const;
	bool SetPreviewAnimation(TObjectPtr<UMotionAnimObject> MotionSequence) const;
	void SetPreviewAnimationNull() const;

	void PreProcessAnimData() const;
	void OpenPickAnimsDialog();

	void CacheTrajectory(const bool bMirrored);

	void CopyPasteNotify(TObjectPtr<UMotionAnimObject> MotionAnimAsset, const FAnimNotifyEvent& InNotify) const;
};