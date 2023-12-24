//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Enumerations/EMMPreProcessEnums.h"
#include "Enumerations/EMotionMatchingEnums.h"
#include "MotionAnimAsset.generated.h"

class UAnimationAsset;
class UAnimSequence;
struct FAnimNotifyEvent;
struct FAnimNotifyEventReference;

/** This is data related to animation sequences used in conjunction with Motion Matching. It is additional data
* held externally to the anim sequence as it only relates to Motion Matching and it is used in the FMotionDataAsset
* struct to store this meta data alongside the used animation sequences.
*/
USTRUCT(BlueprintType)
struct MOTIONSYMPHONY_API FMotionAnimAsset
{
	GENERATED_BODY()

public:
	FMotionAnimAsset();
	FMotionAnimAsset(const FMotionAnimAsset* Copy);
	FMotionAnimAsset(UAnimationAsset* InAnimAsset, class UMotionDataAsset* InParentMotionData);

public:
	/** The Id of this animation within it's type */
	UPROPERTY()
	int32 AnimId;

	/**Identifies the type of motion anim asset this is */
	UPROPERTY()
	EMotionAnimAssetType MotionAnimAssetType;

	/** Does the animation sequence loop seamlessly? */
	UPROPERTY()
	bool bLoop;

	/** The playrate of this animation */
	UPROPERTY()
	float PlayRate;

	/** Should this animation be used in a mirrored form as well? */
	UPROPERTY()
	bool bEnableMirroring;

	/** Should the trajectory be flattened so there is no Y value?*/
	UPROPERTY()
	bool bFlattenTrajectory;

	/** The method for pre-processing the past trajectory beyond the limits of the anim sequence */
	UPROPERTY()
	ETrajectoryPreProcessMethod PastTrajectory;

	/** The method for pre-processing the future trajectory beyond the limits of the anim sequence */
	UPROPERTY()
	ETrajectoryPreProcessMethod FutureTrajectory;

	/** The actual animation asset referenced for this MotionAnimAsset. i.e. AnimSequence or BlendSpace */
	UPROPERTY()
	mutable UAnimationAsset* AnimAsset;

	/** The anim sequence to use for pre-processing motion before the anim sequence if that method is chosen */
	UPROPERTY()
	UAnimSequence* PrecedingMotion;

	/** The anim sequence to use for pre-processing motion after the anim sequence if that method is chosen */
	UPROPERTY()
	UAnimSequence* FollowingMotion;

	/** A cost multiplier for all poses in the animation sequence. The pose cost will be multiplied by this for this anim sequence*/
	UPROPERTY()
	float CostMultiplier;

	/** A list of tag names to be applied to this entire animation. Tags will be converted to enum ID's on a pose level.*/
	UPROPERTY()
	FGameplayTagContainer MotionTags;

	UPROPERTY()
	TArray<FAnimNotifyEvent> Tags;

	UPROPERTY(Transient)
	UMotionDataAsset* ParentMotionDataAsset;

#if WITH_EDITORONLY_DATA
	// if you change Notifies array, this will need to be rebuilt
	UPROPERTY()
	TArray<FAnimNotifyTrack> MotionTagTracks;

#endif // WITH_EDITORONLY_DATA

public:
	virtual ~FMotionAnimAsset();

	virtual float GetPlayRate() const;
	virtual double GetPlayLength() const;
	virtual double GetFrameRate() const;
	virtual int32 GetTickResolution() const;

	/** Playrate adjusted data*/
	virtual double GetRateAdjustedPlayLength() const;
	virtual double GetRateAdjustedFrameRate() const;
	virtual int32 GetRateAdjustedTickResolution() const;

	virtual void PostLoad();

	void SortTags();
	bool RemoveTags(const TArray<FName> & TagsToRemove);

	void GetMotionTags(const float& StartTime, const float& DeltaTime, const bool bAllowLooping, TArray<FAnimNotifyEventReference>& OutActiveNotifies) const;
	virtual void GetMotionTagsFromDeltaPositions(const float& PreviousPosition, const float& CurrentPosition, TArray<FAnimNotifyEventReference>& OutActiveNotifies) const;

	virtual void GetRootBoneTransform(FTransform& OutTransform, const float Time, const bool bMirrored) const;
	virtual void CacheTrajectoryPoints(TArray<FVector>& OutTrajectoryPoints, const bool bMirrored) const;

	void InitializeTagTrack();
	void ClampTagAtEndOfSequence();
	
	uint8* FindTagPropertyData(int32 TagIndex, FArrayProperty*& ArrayProperty);
	uint8* FindArrayProperty(const TCHAR* PropName, FArrayProperty*& ArrayProperty, int32 ArrayIndex);
	
	virtual void RefreshCacheData();

#if WITH_EDITOR
protected:
	DECLARE_MULTICAST_DELEGATE(FOnTagChangedMulticaster);
	FOnTagChangedMulticaster OnTagChanged;

public:
	typedef FOnTagChangedMulticaster::FDelegate FOnTagChanged;

	void RegisterOnTagChanged(const FOnTagChanged& Delegate);
	void UnRegisterOnTagChanged(void* Unregister);
#endif

	// return true if anim Tag is available 
	virtual bool IsTagAvailable() const;
};

USTRUCT()
struct MOTIONSYMPHONY_API FMotionAnimSequence : public FMotionAnimAsset
{
	GENERATED_BODY()

public:
	FMotionAnimSequence();
	FMotionAnimSequence(const FMotionAnimSequence* Copy);
	FMotionAnimSequence(UAnimSequence* InSequence, UMotionDataAsset* InParentMotionData);

public:
	UPROPERTY()
	mutable UAnimSequence* Sequence;

public:
	virtual ~FMotionAnimSequence() override;

	virtual double GetPlayLength() const override;
	virtual double GetFrameRate() const override;

	virtual void GetRootBoneTransform(FTransform& OutTransform, const float Time, const bool bMirrored) const override;
	virtual void CacheTrajectoryPoints(TArray<FVector>& OutTrajectoryPoints, const bool bMirrored) const override;
};

USTRUCT()
struct MOTIONSYMPHONY_API FMotionBlendSpace : public FMotionAnimAsset
{
	GENERATED_BODY()

public:
	FMotionBlendSpace();
	FMotionBlendSpace(const FMotionBlendSpace* Copy);
	FMotionBlendSpace(class UBlendSpace* InBlendSpace, UMotionDataAsset* InParentMotionData);

public:
	UPROPERTY()
	mutable UBlendSpace* BlendSpace;

	UPROPERTY()
	FVector2D SampleSpacing;

public:
	virtual ~FMotionBlendSpace() override;

	virtual double GetPlayLength() const override;
	virtual double GetFrameRate() const override;

	//virtual void GetRootBoneTransform(FTransform& OutTransform, const float Time) const override;
	//virtual void CacheTrajectoryPoints(TArray<FVector>& OutTrajectoryPoints) const override;
};

USTRUCT()
struct MOTIONSYMPHONY_API FMotionComposite : public FMotionAnimAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	mutable class UAnimComposite* AnimComposite;

public:
	FMotionComposite();
	FMotionComposite(const FMotionComposite* Copy);
	FMotionComposite(class UAnimComposite* InComposite, UMotionDataAsset* InParentMotionData);

	virtual ~FMotionComposite() override;

	virtual double GetPlayLength() const override;
	virtual double GetFrameRate() const override;

	virtual void GetRootBoneTransform(FTransform& OutTransform, const float Time, const bool bMirrored) const override;
	virtual void CacheTrajectoryPoints(TArray<FVector>& OutTrajectoryPoints, const bool bMirrored) const override;
};