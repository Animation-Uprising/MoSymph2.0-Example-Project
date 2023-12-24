//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNode_PoseMatchBase.h"
#include "DistanceMatchingNodeData.h"
#include "Animation/AnimSequence.h"
#include "AnimNode_MultiPoseMatching.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FAnimNode_MultiPoseMatching : public FAnimNode_PoseMatchBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TArray<UAnimSequence*> Animations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatching")
	EDistanceMatchingUseCase DistanceMatchingUseCase;

	/**The current desired distance value. +ve if the marker is ahead, -ve if the marker is behind*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatching", meta = (PinShownByDefault))
	float DesiredDistance;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatching")
	FDistanceMatchingNodeData DistanceMatchData;

protected:
	TArray<FDistanceMatchingModule> DistanceMatchingModules;
	FDistanceMatchingModule* MatchDistanceModule;

public:
	FAnimNode_MultiPoseMatching();

	virtual UAnimSequenceBase* FindActiveAnim() override;

#if WITH_EDITOR
	virtual void PreProcess() override;
#endif

protected:
	virtual void InitializeData() override;
	
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	virtual USkeleton* GetNodeSkeleton() override;
	
	virtual int32 GetMinimaCostPoseId(const TArray<float>* InCurrentPoseArray) override;
};