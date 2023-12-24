//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNode_PoseMatchBase.h"
#include "DistanceMatchingNodeData.h"
#include "AnimNode_TransitionMatching.generated.h"

UENUM(BlueprintType)
enum class ETransitionDirectionMethod : uint8
{
	Manual,
	RootMotion
};

UENUM(BlueprintType)
enum class ETransitionMatchingOrder : uint8
{
	TransitionPriority,
	PoseAndTransitionCombined
};

USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FTransitionAnimData
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Animation)
	UAnimSequence* AnimSequence;

	UPROPERTY(EditAnywhere, Category = Animation)
	FVector CurrentMove;

	UPROPERTY(EditAnywhere, Category = Animation)
	FVector DesiredMove;

	UPROPERTY(EditAnywhere, Category = Animation)
	ETransitionDirectionMethod TransitionDirectionMethod;

	UPROPERTY(EditAnywhere, Category = Animation, meta = (ClampMin = 0.0f))
	float CostMultiplier;

	UPROPERTY(EditAnywhere, Category = Animation)
	bool bMirror;

	UPROPERTY()
	int32 StartPose;

	UPROPERTY()
	int32 EndPose;

	UPROPERTY(Transient)
	FDistanceMatchingModule DistanceMatchModule;

public:
	FTransitionAnimData();
	FTransitionAnimData(const FTransitionAnimData& CopyTransition, bool bInMirror = false);
};

USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FAnimNode_TransitionMatching : public FAnimNode_PoseMatchBase
{
	GENERATED_BODY()

public:
	/** Node Input: The current character movement vector relative to the character's current facing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Inputs, meta = (PinShownByDefault))
	FVector CurrentMoveVector;

	/** Node Input: The desired character movement vector relative to the character's current facing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Inputs, meta = (PinShownByDefault))
	FVector DesiredMoveVector;

	/** The order in which transition matching is performed. Transitions directions can either be prioritised
	 or used as a weighting for the cost function **/
	UPROPERTY(EditAnywhere, Category = TransitionSettings)
	ETransitionMatchingOrder TransitionMatchingOrder;

	/** The weighting for the starting direction of the transition **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TransitionSettings)
	float StartDirectionWeight;

	/** The weighting for the final direction of the transition **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TransitionSettings)
	float EndDirectionWeight;

	/** A list of animations as well as their transition data **/
	UPROPERTY(EditAnywhere, Category = Animation)
	TArray<FTransitionAnimData> TransitionAnimData;

	/** How distance matching should be used (if at all) **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatching")
	EDistanceMatchingUseCase DistanceMatchingUseCase;

	/** The current desired distance value. +ve if the marker is ahead, -ve if the marker is behind*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatching", meta = (PinShownByDefault))
	float DesiredDistance;

	/** Contains all data relating to distance matching */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DistanceMatching")
	FDistanceMatchingNodeData DistanceMatchData;

protected:
	UPROPERTY()
	TArray<FTransitionAnimData> MirroredTransitionAnimData;
	
	FDistanceMatchingModule* MatchDistanceModule;
	
public:
	FAnimNode_TransitionMatching();

#if WITH_EDITOR
	virtual void PreProcess() override;
#endif

protected:
	virtual void InitializeData() override;
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	
	virtual void FindMatchPose(const FAnimationUpdateContext& Context) override;
	virtual UAnimSequenceBase* FindActiveAnim() override;

	int32 GetMinimaCostPoseId_TransitionPriority(const TArray<float>& InCurrentPoseArray);
	int32 GetMinimaCostPoseId_PoseTransitionWeighted(const TArray<float>& InCurrentPoseArray);

	int32 GetMinimaCostPoseId_TransitionPriority_Distance();
	int32 GetMinimaCostPoseId_PoseTransitionWeighted_Distance();

	int32 GetAnimationIndex(UAnimSequence* AnimSequence);

	virtual USkeleton* GetNodeSkeleton() override;
};