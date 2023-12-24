//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_RootMotionDecoupler.generated.h"

UENUM(BlueprintType)
enum class EDecouplerClampMode : uint8
{
	None,
	Hard	
};

USTRUCT(BlueprintInternalUseOnly, Experimental)
struct MOTIONSYMPHONY_API FAnimNode_RootMotionDecoupler : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY();

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Settings|Clamping", meta = (PinHiddenByDefault, FoldProperty))
	bool bDecoupleLocation = true;
	
    UPROPERTY(EditAnywhere, Category = "Settings|Clamping", meta = (PinHiddenByDefault, FoldProperty))
	EDecouplerClampMode TranslationClampMode = EDecouplerClampMode::Hard;

	UPROPERTY(EditAnywhere, Category = "Settings|Clamping", meta = (PinHiddenByDefault, FoldProperty))
	float TranslationClampDistance = 30.0f;

	UPROPERTY(EditAnywhere, Category = "Settings|Clamping", meta = (PinHiddenByDefault, FoldProperty))
	bool bDecoupleRotation = true;
	
	UPROPERTY(EditAnywhere, Category = "Settings|Clamping", meta = (PinHiddenByDefault, FoldProperty))
	EDecouplerClampMode RotationClampMode = EDecouplerClampMode::None;

	UPROPERTY(EditAnywhere, Category = "Settings|Clamping", meta = (PinHiddenByDefault, FoldProperty))
	float RotationClampDegrees = 360.0f;
#endif

private:
	FAnimInstanceProxy* AnimInstanceProxy = nullptr;
	float CachedDeltaTime = 0.0f;
	FTransform ComponentTransform_WS = FTransform::Identity;
	FTransform LastRootTransform_WS = FTransform::Identity;
	bool bIsFirstUpdate = true;

	FGraphTraversalCounter UpdateCounter;

public:
	// FAnimNode_Base interface
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
	// End of FAnimNode_Base interface

	// FAnimNode_SkeletalControlBase interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;
	// End of FAnimNode_SkeletalControlBase interface

	// Folded property accesors
	bool IsDecoupleLocation() const;
	EDecouplerClampMode GetTranslationClampMode() const;
	float GetTranslationClampDistance() const;
	bool IsDecoupleRotation() const;
	EDecouplerClampMode GetRotationClampMode() const;
	float GetRotationClampDegrees() const;

private:
	void Reset(const FAnimationBaseContext& Context);
};

