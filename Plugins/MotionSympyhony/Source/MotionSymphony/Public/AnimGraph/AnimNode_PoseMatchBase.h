//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNode_MotionRecorder.h"
#include "Data/CalibrationData.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimNode_PoseMatchBase.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FPoseMatchData
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY()
	int32 PoseId;

	UPROPERTY()
	int32 AnimId;

	UPROPERTY()
	bool bMirror;

	UPROPERTY()
	float Time;

public:
	FPoseMatchData();
	FPoseMatchData(int32 InPoseId, int32 InAnimId, float InTime, bool bMirror);
};

USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FAnimNode_PoseMatchBase : public FAnimNode_SequencePlayer
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PoseMatching, meta = (ClampMin = 0.01f))
	float PoseInterval;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PoseMatching, meta = (ClampMin = 0.01f))
	float PosesEndTime;
	
	UPROPERTY(EditAnywhere, Category = PoseConfiguration)
	TObjectPtr<UMotionMatchConfig> PoseConfig;

	UPROPERTY(EditAnywhere, Category = PoseConfiguration)
	TObjectPtr<UMotionCalibration> Calibration;

	UPROPERTY(EditAnywhere, Category = Mirroring)
	bool bEnableMirroring;

	UPROPERTY(EditAnywhere, Category = Mirroring)
	TObjectPtr<UMirrorDataTable> MirrorDataTable = nullptr;

protected:
	//bool bPreProcessed;
	bool bInitialized;
	bool bInitPoseSearch;
	int32 PoseRecorderConfigIndex;

	//Baked poses
	UPROPERTY()
	TArray<FPoseMatchData> Poses;

	UPROPERTY()
	FCalibrationData FinalCalibration;
	
	UPROPERTY()
	TArray<float> PoseMatrix;

	UPROPERTY()
	FCalibrationData StandardDeviation;

	//Pose Data extracted from Motion Recorder

	//The chosen animation data
	FPoseMatchData* MatchPose;
	int32 MatchPoseIndex;

	FAnimInstanceProxy* AnimInstanceProxy;

private:
	bool bIsDirtyForPreProcess;
	
	//Compact pose format of mirror bone map
	TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex> CompactPoseMirrorBones;

	//Pre-calculated component space to reference pose, which allows mirror to work with any joint orientation
	TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> ComponentSpaceRefRotations;

public:
	FAnimNode_PoseMatchBase();
	
	virtual void PreProcess();
	void SetDirtyForPreProcess();

protected:
	virtual void PreProcessAnimation(UAnimSequence* Anim, int32 AnimIndex, bool bMirror = false);
	virtual void InitializeData();
	virtual void FindMatchPose(const FAnimationUpdateContext& Context); 
	virtual UAnimSequenceBase*	FindActiveAnim();
	virtual int32 GetMinimaCostPoseId(const TArray<float>* InCurrentPoseArray);
	int32 GetMinimaCostPoseId(const TArray<float>& InCurrentPoseArray, float& OutCost, int32 InStartPoseId, int32 InEndPoseId);
	float ComputeSinglePoseCost(const TArray<float>& InCurrentPoseArray, const int32 InPoseIndex);
	
	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	// End of FAnimNode_Base interface

	virtual USkeleton* GetNodeSkeleton();

private:
	void PreProcessAnimPass(UAnimSequence* Anim, const float AnimLength, const int32 AnimIndex, const bool bMirror);
	void FillCompactPoseAndComponentRefRotations(const FBoneContainer& BoneContainer);
	void DrawPoseMatchDebug(const FAnimInstanceProxy* InAnimInstanceProxy);
	
};