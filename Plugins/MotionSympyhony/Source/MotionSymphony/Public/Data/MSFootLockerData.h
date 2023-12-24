//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneContainer.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "MSFootLockerData.generated.h"

/** An enumeration defining the various options for getting the stride warp vector */
UENUM()
enum class EMSStrideVectorMethod : uint8
{
	ManualVelocity UMETA(DisplayName = "Manual Input", ToolTip = "Stride direction must be set manually in the animation graph, either by a pin or fixed settigns."),
	ActorVelocity UMETA(DisplayName = "Actor Velocity", ToolTip = "Stride direction will be automatically calculated from the actor's current velocity.")
};

struct FSocketReference;

/** The data structure to define Two Bone IK limbs for foot locking + additional slots for foot placement*/
USTRUCT(BlueprintType)
struct FMSFootLockLimbDefinition
{
	GENERATED_BODY()

public:
	/** Reference to the tip bone of your limb (e.g. foot bone) */
	UPROPERTY(EditAnywhere, Category = LimbSettings)
	FBoneReference FootBone;

	/** Foot locker locks the toe, not the foot, so a reference to the toe bone is required here. */
	UPROPERTY(EditAnywhere, Category = LimbSettings)
	FBoneReference ToeBone;

	/** Reference to the IkTarget bone (e.g. ikFoot). Only required for 'External' or 'Both' Ik method */
	UPROPERTY(EditAnywhere, Category = LimbSettings)
	FBoneReference IkTarget;

	/** The number of bones in the IK Chain (usually 2 for a normal human leg). This must not include the toe */
	UPROPERTY(EditAnywhere, Category = LimbSettings, meta = (ClampMin = 2))
	int32 LimbBoneCount;


public:
	TArray<FBoneReference> Bones;
	float LengthSqr;
	float Length;
	float HeightDelta;

	FVector ToeLocation_LS;

	FVector ToeLocation_CS;
	FVector FootLocation_CS;
	FTransform BaseBoneTransform_CS;

public:
	FMSFootLockLimbDefinition();
	void Initialize(const FBoneContainer& PoseBones, const FAnimInstanceProxy* InAnimInstanceProxy);
	bool IsValid(const FBoneContainer& PoseBones);
	void CalculateLength(FCSPose<FCompactPose>& Pose);
	void CalculateFootToToeOffset(FCSPose<FCompactPose>& Pose);
};