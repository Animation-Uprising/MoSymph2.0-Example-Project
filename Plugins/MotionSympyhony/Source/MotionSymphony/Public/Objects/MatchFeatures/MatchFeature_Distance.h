//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MatchFeatureBase.h"
#include "EDistanceMatchingEnums.h"
#include "MatchFeature_Distance.generated.h"

UCLASS(BlueprintType)
class MOTIONSYMPHONY_API UMatchFeature_Distance : public UMatchFeatureBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Match Feature|Distance Matching")
	EDistanceMatchTrigger DistanceMatchTrigger;

	UPROPERTY(EditAnywhere, Category = "Match Feature|Distance Matching")
	EDistanceMatchType DistanceMatchType;

	UPROPERTY(EditAnywhere, Category = "Match Feature|Distance Matching")
	EDistanceMatchBasis DistanceMatchBasis;

public:
	UMatchFeature_Distance();
	virtual bool IsSetupValid() const override;
	virtual int32 Size() const override;

	virtual void EvaluatePreProcess(float* ResultLocation, UAnimSequence* InSequence,
	                                const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, ::TObjectPtr<
	                                UMotionAnimObject> InMotionObject) override;
	virtual void EvaluatePreProcess(float* ResultLocation, UAnimComposite* InComposite,
	                                const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, TObjectPtr<
	                                UMotionAnimObject> InAnimObject) override;
	virtual void EvaluatePreProcess(float* ResultLocation, UBlendSpace* InBlendSpace,
	                                const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, const FVector2D BlendSpacePosition, TObjectPtr
	                                <UMotionAnimObject>
	                                InAnimObject) override;

	virtual void SourceInputData(TArray<float>& OutFeatureArray, const int32 FeatureOffset, AActor* InActor) override;
	virtual bool NextPoseToleranceTest(const TArray<float>& DesiredInputArray, const TArray<float>& PoseMatrix,
		const int32 MatrixStartIndex, const int32 FeatureOffset, const float PositionTolerance, const float RotationTolerance) override;
	
	virtual bool CanBeResponseFeature() const override;
	
#if WITH_EDITOR
	 virtual void DrawPoseDebugEditor(UMotionDataAsset* MotionData, UDebugSkelMeshComponent* DebugSkeletalMesh,
	 	const int32 PreviewIndex, const int32 FeatureOffset, const UWorld* World, FPrimitiveDrawInterface* DrawInterface) override;
	// virtual void DrawDebugDesiredRuntime(FAnimInstanceProxy* AnimInstanceProxy, FMotionMatchingInputData& InputData,
	// 	const int32 FeatureOffset, UMotionMatchConfig* MMConfig) override;
	// virtual void DrawDebugCurrentRuntime(FAnimInstanceProxy* AnimInstanceProxy, UMotionDataAsset* MotionData,
	// 	TArray<float>& CurrentPoseArray, const int32 FeatureOffset)  override;
#endif

private:
	bool DoesTagMatch(const class UTag_DistanceMarker* InTag) const;
	float ExtractSqrDistanceToMarker(const FTransform& InRootTransform) const;

	static FTransform ExtractBlendSpaceRootMotion(const UBlendSpace* InBlendSpace,
		const FVector2D InBlendSpacePosition, const float InBaseTime, const float InDeltaTime);

	static FTransform ExtractCompositeRootMotion(const class UAnimComposite* InAnimComposite,
		const float InBaseTime, const float InDeltaTime);
};
