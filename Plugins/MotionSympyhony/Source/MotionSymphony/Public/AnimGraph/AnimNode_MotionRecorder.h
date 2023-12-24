//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimNodeMessages.h"
#include "AnimNode_MotionRecorder.generated.h"

class UMotionMatchConfig;

class MOTIONSYMPHONY_API IMotionSnapper : public UE::Anim::IGraphMessage
{
	DECLARE_ANIMGRAPH_MESSAGE(IMotionSnapper);

public:
	static const FName Attribute;

	virtual struct FAnimNode_MotionRecorder& GetNode() = 0;
	virtual void AddDebugRecord(const FAnimInstanceProxy& InSourceProxy, int32 InSourceNodeId) = 0;
};

USTRUCT()
struct MOTIONSYMPHONY_API FMotionRecordData
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TObjectPtr<UMotionMatchConfig> MotionMatchConfig;

	UPROPERTY(Transient)
	TArray<float> RecordedPoseArray;

	UPROPERTY(Transient)
	TArray<float> FeatureCacheData;

public:
	FMotionRecordData();
	FMotionRecordData(UMotionMatchConfig* InMotionMatchConfig);
};


USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FAnimNode_MotionRecorder : public FAnimNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links)
	FPoseLink Source;

	UPROPERTY(EditAnywhere, Category = Settings)
	bool bRetargetPose;
	
	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<TObjectPtr<UMotionMatchConfig>> MotionConfigs;

private:
	float PoseDeltaTime;
	FAnimInstanceProxy* AnimInstanceProxy;
	
	TArray<FMotionRecordData> MotionRecorderData;

public:

	FAnimNode_MotionRecorder();
	void CacheMotionBones(const FAnimInstanceProxy* InProxy);
	const TArray<float>* GetCurrentPoseArray(const UMotionMatchConfig* InConfig);
	const TArray<float>* GetCurrentPoseArray(const int32 ConfigIndex);
	int32 GetMotionConfigIndex(const UMotionMatchConfig* InConfig);
	int32 RegisterMotionMatchConfig(UMotionMatchConfig* InMotionMatchConfig);
	
	static void LogRequestError(const FAnimationUpdateContext& Context, const FPoseLinkBase& RequesterPoseLink);

public: 
	// FAnimNode_Base
	virtual bool NeedsOnInitializeAnimInstance() const override { return true; } 
	virtual void OnInitializeAnimInstance(const FAnimInstanceProxy* InProxy, const UAnimInstance* InAnimInstance) override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base
};