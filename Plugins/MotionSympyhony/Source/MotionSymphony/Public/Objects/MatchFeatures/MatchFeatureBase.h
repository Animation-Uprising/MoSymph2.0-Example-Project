//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimComposite.h"
#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"
#include "BonePose.h"
#include "MatchFeatureBase.generated.h"

class UMotionDataAsset;
class UMotionAnimObject;
class UMotionMatchConfig;
struct FMotionMatchingInputData;
class UMirroringProfile;
struct FMotionAnimSequence;
struct FMotionComposite;
struct FMotionBlendSpace;
struct FPoseMotionData;

UENUM()
enum class EPoseCategory : uint8
{
	Quality,
	Responsiveness
};

struct FPoseMatrix;

/** Base class for all motion matching features */
UCLASS(abstract, EditInlineNew, config=Game)
class MOTIONSYMPHONY_API UMatchFeatureBase : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Match Feature");
	float DefaultWeight;

	UPROPERTY();
	EPoseCategory PoseCategory;
	

public:
	UMatchFeatureBase();
	virtual void Initialize();
	virtual bool IsSetupValid() const;
	virtual bool IsMotionSnapshotCompatible() const;
	
	virtual int32 Size() const;

	/** Pre Processing*/
	virtual void EvaluatePreProcess(float* ResultLocation, UAnimSequence* InSequence,
	                                const float Time, const float PoseInterval, const bool bMirror,
	                                UMirrorDataTable* MirrorDataTable, TObjectPtr<UMotionAnimObject> InMotionObject);
	virtual void EvaluatePreProcess(float* ResultLocation, UAnimComposite* InComposite,
	                                const float Time, const float PoseInterval, const bool bMirror, UMirrorDataTable* MirrorDataTable, TObjectPtr<
	                                UMotionAnimObject> InAnimObject);
	virtual void EvaluatePreProcess(float* ResultLocation, UBlendSpace* InBlendSpace,
	                                const float Time, const float PoseInterval, const bool bMirror,
	                                UMirrorDataTable* MirrorDataTable, const FVector2D BlendSpacePosition, TObjectPtr<UMotionAnimObject> InAnimObject);
	virtual void CleanupPreProcessData();
	/** End Pre-Processing*/

	virtual void CacheMotionBones(const FAnimInstanceProxy* InAnimInstanceProxy);
	virtual void ExtractRuntime(FCSPose<FCompactPose>& CSPose, float* ResultLocation, float* FeatureCacheLocation, FAnimInstanceProxy*
	                            AnimInstanceProxy, float DeltaTime);
	

	//Input Response Functions
	virtual void SourceInputData(TArray<float>& OutFeatureArray, const int32 FeatureOffset, AActor* InActor);
	virtual void ApplyInputBlending(TArray<float>& DesiredInputArray, const TArray<float>& CurrentPoseArray, const int32 FeatureOffset, const float Weight);
	virtual bool NextPoseToleranceTest(const TArray<float>& DesiredInputArray, const TArray<float>& PoseMatrix,
	                                   const int32 MatrixStartIndex, const int32 FeatureOffset, const float PositionTolerance, const float RotationTolerance);

	virtual float GetDefaultWeight(int32 AtomId) const;

	virtual void CalculateDistanceSqrToMeanArrayForStandardDeviations(TArray<float>& OutDistToMeanSqrArray,
	                                                                  const TArray<float>& InMeanArray, const TArray<float>& InPoseArray, const int32 FeatureOffset, const int32 PoseStartIndex) const;

	virtual bool CanBeQualityFeature() const;
	virtual bool CanBeResponseFeature() const;

#if WITH_EDITOR
	//Draws debugging for this feature in the MoSymph editor for the current pose . (E.g. draw baked trajectory data)
	virtual void DrawPoseDebugEditor(UMotionDataAsset* MotionData, UDebugSkelMeshComponent* DebugSkeletalMesh,
		const int32 PreviewIndex, const int32 FeatureOffset, const UWorld* World, FPrimitiveDrawInterface* DrawInterface); 

	//Draws debugging for this feature in the MoSymph editor for the entire animation . (E.g. draw entire trajectory for whole animation)
	virtual void DrawAnimDebugEditor(); 

	//Draws debugging for this feature at runtime for desired input array (used mostly for responsive category. (E.g.Draw desired trajectory at runtime)
	virtual void DrawDebugDesiredRuntime(FAnimInstanceProxy* AnimInstanceProxy, FMotionMatchingInputData& InputData,
		const int32 FeatureOffset, UMotionMatchConfig* MMConfig); 

	//Draws debugging for this feature at runtime for the chosen pose. (E.g. Draw chosen trajectory at runtime)
	virtual void DrawDebugCurrentRuntime(FAnimInstanceProxy* AnimInstanceProxy, TObjectPtr<const UMotionDataAsset> MotionData,
	                                     const TArray<float>& CurrentPoseArray, const int32 FeatureOffset);
#endif
	
	bool operator <(const UMatchFeatureBase& other);
};