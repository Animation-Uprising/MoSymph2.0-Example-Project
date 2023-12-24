//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Animation/AnimationAsset.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimComposite.h"
#include "Data/PoseMatrixAABB.h"
#include "Enumerations/EMotionMatchingEnums.h"
#include "Enumerations/EMMPreProcessEnums.h"
#include "Data/PoseMotionData.h"
#include "Data/CalibrationData.h"
#include "Data/MotionAnimAsset.h"
#include "Objects/Assets/MotionMatchConfig.h"
#include "Data/PoseMatrix.h"
#include "MotionDataAsset.generated.h"

class UMotionAnimObject;
class UMotionCompositeObject;
class UMotionSequenceObject;
class UMotionBlendSpaceObject;
class USkeleton;
struct FAnimChannelState;

/** This is a custom animation asset used for pre-processing and storing motion matching animation data.
 * It is used as the source asset to 'play' with the 'Motion Matching' animation node and is part of the
 * Motion Symphony suite of animation tools.
 */
UCLASS(BlueprintType, HideCategories = ("Animation", "Thumbnail"))
class MOTIONSYMPHONY_API UMotionDataAsset : public UAnimationAsset
{
	GENERATED_BODY()

public:
	/** The time, in seconds, between each pose recorded in the pre-processing stage (0.05 - 0.1 recommended)*/
	UPROPERTY(EditAnywhere, Category = "Motion Matching", meta = (ClampMin = 0.01f, ClampMax = 0.5f))
	float PoseInterval;

	/** The configuration to use for this motion data asset. This includes the skeleton, trajectory points and 
	pose joints to match when pre-processing and at runtime. Use the same configuration for this asset as you
	do on the runtime node.*/
	UPROPERTY(EditAnywhere, Category = "Motion Matching")
	TObjectPtr<UMotionMatchConfig> MotionMatchConfig;

	/** The method to be used for calculating joint velocity. Most of the time this should be left as default */
	UPROPERTY(EditAnywhere, Category = "Motion Matching")
	EJointVelocityCalculationMethod JointVelocityCalculationMethod;

	/** The rules for triggering notifies on animations played by the motion matching node*/
	UPROPERTY(EditAnywhere, Category = AnimationNotifies)
	TEnumAsByte<ENotifyTriggerMode::Type> NotifyTriggerMode;

	UPROPERTY(EditAnywhere, Category = "Motion Matching|Mirroring")
	TObjectPtr<UMirrorDataTable> MirrorDataTable = nullptr;

	/** Has the Motion Data been processed before the last time it's data was changed*/
	UPROPERTY()
	bool bIsProcessed;

	/** A list of all source animations used for this MotionData asset along with meta data 
	related to the animation sequence for pre-processing and runtime purposes.*/
	UPROPERTY()
	TArray<FMotionAnimSequence> SourceMotionAnims;
	
	/** A list of all source blend spaces used for this Motion Data asset along with meta data 
	related to the blend space for pre-processing and runtime purposes*/
	UPROPERTY()
	TArray<FMotionBlendSpace> SourceBlendSpaces;
	
	UPROPERTY()
	TArray<FMotionComposite> SourceComposites;

	UPROPERTY(Instanced)
	TArray<TObjectPtr<UMotionSequenceObject>> SourceMotionSequenceObjects;

	UPROPERTY(Instanced)
	TArray<TObjectPtr<UMotionBlendSpaceObject>> SourceBlendSpaceObjects;

	UPROPERTY(Instanced)
	TArray<TObjectPtr<UMotionCompositeObject>> SourceCompositeObjects;
	
	/** Remaps the pose ID in the pose database to the pose Id in the pose array. This is so that DoNotUse
	 * Poses can be removed from the PoseArray */
	UPROPERTY()
	TArray<int32> PoseIdRemap;

	/** Remaps the pose Id in the pose array to the pose id in the pose database. This is the reverse of
	 * PoseIdRemap so that remaps can be done in both directions */
	UPROPERTY()
	TMap<int32, int32> PoseIdRemapReverse;

	UPROPERTY()
	TArray<FGameplayTagContainer> MotionTagList;
	
	UPROPERTY()
	TArray<FPoseMatrixSection> MotionTagMatrixSections;

	/**Map of calibration data for normalizing all atoms. This stores the standard deviation of all atoms throughout the data set
	but separates them via motion trait. There is one feature standard deviation per motion trait field. */
	UPROPERTY()
	TArray<FCalibrationData> FeatureStandardDeviations;
	
	/** A list of all poses generated during the pre-process stage. Each pose contains information
	about an animation frame within the animation data set.*/
	UPROPERTY()
	TArray<FPoseMotionData> Poses;
	
	/** The pose matrix, all pose data represented in a single linear array of floats*/
	UPROPERTY()
	FPoseMatrix LookupPoseMatrix;

	/** An AABB data structure used to assist with searching through the pose matrix*/
	UPROPERTY(Transient)
	FPoseAABBMatrix PoseAABBMatrix_Outer;

	UPROPERTY(Transient)
	FPoseAABBMatrix PoseAABBMatrix_Inner;
	
	/** The searchable pose matrix, contains only pose data that is searchable with flagged poses removed*/
	UPROPERTY(Transient)
	FPoseMatrix SearchPoseMatrix;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TArray<UObject*> MotionSelection;

	int32 MaxMotionSelectionSize = 0;

	/** The index of the Anim currently being previewed.*/
	int32 AnimPreviewIndex;

	EMotionAnimAssetType AnimMetaPreviewType;
#endif

public:
	UMotionDataAsset(const FObjectInitializer& ObjectInitializer);
	
	//Anim Assets
	int32 GetAnimCount() const;
	int32 GetSourceAnimCount() const;
	int32 GetSourceBlendSpaceCount() const;
	int32 GetSourceCompositeCount() const;
	TObjectPtr<const UMotionAnimObject> GetSourceAnim(const int32 AnimId, const EMotionAnimAssetType AnimType);
	TObjectPtr<const UMotionSequenceObject> GetSourceSequenceAtIndex(const int32 AnimIndex) const;
	TObjectPtr<const UMotionBlendSpaceObject> GetSourceBlendSpaceAtIndex(const int32 BlendSpaceIndex) const;
	TObjectPtr<const UMotionCompositeObject> GetSourceCompositeAtIndex(const int32 CompositeIndex) const;
	TObjectPtr<UMotionAnimObject> GetEditableSourceAnim(const int32 AnimId, const EMotionAnimAssetType AnimType);
	TObjectPtr<UMotionSequenceObject> GetEditableSourceSequenceAtIndex(const int32 AnimIndex);
	TObjectPtr<UMotionBlendSpaceObject> GetEditableSourceBlendSpaceAtIndex(const int32 BlendSpaceIndex);
	TObjectPtr<UMotionCompositeObject> GetEditableSourceCompositeAtIndex(const int32 CompositeIndex);
	
	void AddSourceAnim(UAnimSequence* AnimSequence);
	void AddSourceBlendSpace(UBlendSpace* BlendSpace);
	void AddSourceComposite(UAnimComposite* Composite);
	bool IsValidSourceAnimIndex(const int32 AnimIndex) const;
	bool IsValidSourceBlendSpaceIndex(const int32 BlendSpaceIndex) const;
	bool IsValidSourceCompositeIndex(const int32 CompositeIndex) const;
	void DeleteSourceAnim(const int32 AnimIndex);
	void DeleteSourceBlendSpace(const int32 BlendSpaceIndex);
	void DeleteSourceComposite(const int32 CompositeIndex);
	void ClearSourceAnims();
	void ClearSourceBlendSpaces();
	void ClearSourceComposites();
	void GenerateSearchPoseMatrix(); //Generates a pose matrix that can be used for searches

	//General
	bool CheckValidForPreProcess() const;
	void PreProcess();
	void ClearPoses();
	bool IsSetupValid();
	bool AreSequencesValid();
	float GetPoseInterval() const;
	float GetPoseFavour(const int32 PoseId) const;
	int32 GetMotionTagIndex(const FGameplayTagContainer& MotionTags) const;
	int32 GetMotionTagStartPoseIndex(const FGameplayTagContainer& MotionTags) const;
	int32 GetMotionTagEndPoseIndex(const FGameplayTagContainer& MotionTags) const;
	void GetMotionTagStartAndEndPoseIndex(const FGameplayTagContainer& MotionTags, int32& OutStartIndex, int32& OutEndIndex) const;
	void FindMotionTagRangeIndices(const FGameplayTagContainer& MotionTags, int32& OutStartIndex, int32& OutEndIndex) const;
	int32 MatrixPoseIdToDatabasePoseId(int32 MatrixPoseId) const;
	int32 DatabasePoseIdToMatrixPoseId(int32 DatabasePoseId) const;
	bool IsSearchPoseMatrixGenerated() const;
	
	
	/** UObject Interface*/
	virtual void PostLoad() override;
	/** End UObject Interface*/

	/** UAnimationAsset interface */
	virtual void Serialize(FArchive& Ar) override;
	/** End UAnimationAsset interface */

	//~ Begin UAnimationAsset Interface
#if WITH_EDITOR
	virtual void RemapTracksToNewSkeleton(USkeleton* NewSkeleton, bool bConvertSpaces) override;
#endif
	virtual void TickAssetPlayer(FAnimTickRecord& Instance, struct FAnimNotifyQueue& NotifyQueue, FAnimAssetTickContext& Context) const override;

	virtual void TickAnimChannelForSequence(const FAnimChannelState& ChannelState, FAnimAssetTickContext& Context,
	                                        TArray<FAnimNotifyEventReference>& Notifies, const float DeltaTime, const bool bGenerateNotifies) const;

	virtual void TickAnimChannelForBlendSpace(const FAnimChannelState& ChannelState,
	                                          FAnimAssetTickContext& Context, TArray<FAnimNotifyEventReference>& Notifies, const float DeltaTime, const bool bGenerateNotifies) const;

	virtual void TickAnimChannelForComposite(const FAnimChannelState& ChannelState, FAnimAssetTickContext& Context,
	                                         TArray<FAnimNotifyEventReference>& Notifies, const float DeltaTime, const bool bGenerateNotifies) const;

	virtual void SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty = true) override;
	virtual USkeletalMesh* GetPreviewMesh(bool bMarkAsDirty = true);
	virtual USkeletalMesh* GetPreviewMesh() const;
	virtual void RefreshParentAssetData();
	virtual float GetMaxCurrentTime();
#if WITH_EDITOR	
	virtual bool GetAllAnimationSequencesReferred(TArray<class UAnimationAsset*>& AnimationSequences, bool bRecursive = true);
	virtual void ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap) override;
	//~ End UAnimationAsset Interface

	//void MotionAnimMetaDataModified();
	bool SetAnimPreviewIndex(EMotionAnimAssetType CurAnimType, int32 CurAnimId);
#endif

#if WITH_EDITORONLY_DATA
	TArray<UObject*>& GetSelectedMotions();
	void AddSelectedMotion(const int32 AnimIndex, const EMotionAnimAssetType AnimType);
	void ClearMotionSelection();
#endif

private:
	void AddAnimNotifiesToNotifyQueue(FAnimNotifyQueue& NotifyQueue, TArray<FAnimNotifyEventReference>& Notifies, float InstanceWeight) const;

	/** Calculates the Atom count per pose and total pose count and then zero fills the pose matrix to fit all poses*/
	void InitializePoseMatrix();

	void PreProcessAnim(const int32 SourceAnimIndex, const bool bMirror = false);
	void PreProcessBlendSpace(const int32 SourceBlendSpaceIndex, const bool bMirror = false);
	void PreProcessComposite(const int32 SourceCompositeIndex, const bool bMirror = false);
	void GeneratePoseSequencing();
	void MarkEdgePoses(float InMaxAnimBlendTime);
	
};
