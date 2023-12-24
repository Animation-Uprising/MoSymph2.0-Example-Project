//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "BonePose.h"
#include "Animation/AnimInstanceProxy.h"
#include "Data/MSFootLockerData.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_MSFootLocker.generated.h"

UENUM()
enum class EMSHyperExtensionFixMethod : uint8
{
	None,
	MoveFootTowardsThigh,
	MoveFootUnderThigh
};

/** A runtime animation node for warping/scaling the stride of characters */
USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FAnimNode_MSFootLocker : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Inputs, meta = (PinHiddenByDefault))
	bool bLeftFootLock;

	UPROPERTY(EditAnywhere, Category = Inputs, meta = (PinHiddenByDefault))
	bool bRightFootLock;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EMSHyperExtensionFixMethod LegHyperExtensionFixMethod;
	
	/** By default strider will only ever compress legs never extend them. This percentage (0.0 - 1.0) allows for 
	leg extension up to but not beyond a straight leg. It is the percentage of the remaining available leg extension.*/
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = 0.0f, ClampMax = 0.99f))
	float AllowLegExtensionRatio;

	/** The amount of time (seconds) taken for the weight of a foot lock to be released. This allows for a smooth
	transition back to unlocked animation when the lock is released. A negative value will result in no smoothing. */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = -1.0f))
	float LockReleaseSmoothTime;

	/** A data structure defining the left foot and leg for the purposes of foot locking */ 
	UPROPERTY(EditAnywhere, Category = BodyDefinition)
	FMSFootLockLimbDefinition LeftFootDefinition;

	/** A data structure defining the right foot and leg for the purposes of foot locking */
	UPROPERTY(EditAnywhere, Category = BodyDefinition)
	FMSFootLockLimbDefinition RightFootDefinition;
	
private:
	float DeltaTime;
	bool bValidCheckResult;

	float LeftFootLockWeight;
	float RightFootLockWeight;

	bool bLeftFootLock_LastFrame;
	bool bRightFootLock_LastFrame;

	FVector LeftLockLocation_WS;
	FVector RightLockLocation_WS;

	FAnimInstanceProxy* AnimInstanceProxy;

public:
	FAnimNode_MSFootLocker();

private:
	virtual void EvaluateFootLocking(FComponentSpacePoseContext& Output,
		TArray<FBoneTransform>& OutBoneTransforms);

	bool CheckValidBones(const FBoneContainer& RequiredBones);

public:
	//FAnimNode_SkeletalControlBase interface
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, 
		TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	//End of FAnimNode_SkeletalControlBase interface

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override;
	// End of FAnimNode_Base_Interface

private:
	//FAnimNode_SkeletalControlBase interface
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;
	//End of FAnimNode_SkeletalControlBase interface

	void EvaluateLimb(TArray<FBoneTransform>& OutBoneTransforms, FComponentSpacePoseContext& Output,
	                  const FTransform& ComponentTransform_WS, FMSFootLockLimbDefinition& LimbDefinition,
	                  FVector& LockLocation_WS, float& FootLockWeight, bool bLockFoot, bool& bLockFoot_LastFrame, const int32 DebugLevel);
};