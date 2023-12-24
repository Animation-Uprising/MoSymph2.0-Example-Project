//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MotionAnimObject.generated.h"

class UAnimComposite;
enum class EMotionAnimAssetType : uint8;
struct FMotionAnimAsset;
enum class ETrajectoryPreProcessMethod : uint8;
class UMotionDataAsset;

UCLASS(EditInlineNew, DefaultToInstanced, config=Game, HideCategories=("Object"))
class MOTIONSYMPHONY_API UMotionAnimObject : public UObject
{
	GENERATED_BODY()

public:
	/** The Id of this animation within it's type */
	UPROPERTY()
	int32 AnimId;

	/**Identifies the type of motion anim asset this is */
	UPROPERTY()
	EMotionAnimAssetType MotionAnimAssetType;

	UPROPERTY()
	mutable UAnimationAsset* AnimAsset;
	
	/** Does the animation sequence loop seamlessly? */
	UPROPERTY(EditAnywhere, Category = "General")
	bool bLoop;

	/** The playrate for this animation */
	UPROPERTY(EditAnywhere, Category = "General", meta = (ClampMin = 0.01f, ClampMax = 10.0f))
	float PlayRate;

	/** Should this animation be used in a mirrored form as well? */
	UPROPERTY(EditAnywhere, Category = "General")
	bool bEnableMirroring;

	/** Should the trajectory be flattened so there is no Y value?*/
	UPROPERTY(EditAnywhere, Category = "Pre Process")
	bool bFlattenTrajectory;

	/** The method for pre-processing the past trajectory beyond the limits of the anim sequence */
	UPROPERTY(EditAnywhere, Category = "Pre Process")
	ETrajectoryPreProcessMethod PastTrajectory;

	/** The anim sequence to use for pre-processing motion before the anim sequence if that method is chosen */
	UPROPERTY(EditAnywhere, Category = "Pre Process")
	TObjectPtr<UAnimSequence> PrecedingMotion;

	/** The method for pre-processing the future trajectory beyond the limits of the anim sequence */
	UPROPERTY(EditAnywhere, Category = "Pre Process")
	ETrajectoryPreProcessMethod FutureTrajectory;

	/** The anim sequence to use for pre-processing motion after the anim sequence if that method is chosen */
	UPROPERTY(EditAnywhere, Category = "Pre Process")
	TObjectPtr<UAnimSequence> FollowingMotion;

	/** The favour for all poses in the animation sequence. The pose cost will be multiplied by this for this anim sequence */
	UPROPERTY(EditAnywhere, Category = "Tags")
	float CostMultiplier;

	/** A list of tags that this animation will be given for it's entire duration. Tag names will be converted to enum during pre-process.*/
	UPROPERTY(EditAnywhere, Category = "Tags")
	FGameplayTagContainer MotionTags;

	UPROPERTY()
	TArray<FAnimNotifyEvent> Tags;
	
	UPROPERTY(Transient)
	TObjectPtr<UMotionDataAsset> ParentMotionDataAsset;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FAnimNotifyTrack> MotionTagTracks;
#endif

public:
	UMotionAnimObject(const FObjectInitializer& ObjectInitializer);
	void InitializeBase(const UMotionAnimObject* Copy);
	void InitializeBase(const int32 InAnimId, TObjectPtr<UAnimationAsset> InAnimAsset, TObjectPtr<UMotionDataAsset> InParentMotionDataAsset);
	
	virtual void InitializeFromLegacy(FMotionAnimAsset* LegacyMotion);
	
	virtual float GetPlayRate() const;
	virtual double GetPlayLength() const;
	virtual double GetFrameRate() const;
	virtual int32 GetTickResolution() const;

	/** Playrate adjusted data*/
	virtual double GetRateAdjustedPlayLength() const;
	virtual double GetRateAdjustedFrameRate() const;
	virtual int32 GetRateAdjustedTickResolution() const;

	virtual void PostLoad() override;

	void SortTags();
	bool RemoveTags(const TArray<FName> & TagsToRemove);

	void GetMotionTags(const float& StartTime, const float& DeltaTime, const bool bAllowLooping,
		TArray<FAnimNotifyEventReference>& OutActiveNotifies) const;
	virtual void GetMotionTagsFromDeltaPositions(const float& PreviousPosition, const float& CurrentPosition,
	                                             TArray<FAnimNotifyEventReference>& OutActiveTags) const;

	virtual void GetRootBoneTransform(FTransform& OutTransform, const float Time, const bool bMirrored) const;
	virtual void CacheTrajectoryPoints(TArray<FVector>& OutTrajectoryPoints, const bool bMirrored) const;

	void InitializeTagTrack();
	void ClampTagAtEndOfSequence();

	virtual void RefreshCacheData();

#if WITH_EDITORONLY_DATA	
	uint8* FindTagPropertyData(int32 TagIndex, FArrayProperty*& ArrayProperty);
	uint8* FindArrayProperty(const TCHAR* PropName, FArrayProperty*& ArrayProperty, int32 ArrayIndex);

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

UCLASS(EditInlineNew, DefaultToInstanced, config=Game,HideCategories=("Object"))
class MOTIONSYMPHONY_API UMotionSequenceObject : public UMotionAnimObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	mutable UAnimSequence* Sequence;

public:
	UMotionSequenceObject(const FObjectInitializer& ObjectInitializer);
	void Initialize(const UMotionSequenceObject* Copy);
	void Initialize(const int32 InAnimId, TObjectPtr<UAnimSequence> InSequence, TObjectPtr<UMotionDataAsset> InParentMotionData);
	virtual void InitializeFromLegacy(FMotionAnimAsset* LegacyMotion) override;

	virtual double GetPlayLength() const override;
	virtual double GetFrameRate() const override;

	virtual void GetRootBoneTransform(FTransform& OutTransform, const float Time, const bool bMirrored) const override;
	virtual void CacheTrajectoryPoints(TArray<FVector>& OutTrajectoryPoints, const bool bMirrored) const override;
};

UCLASS(EditInlineNew, DefaultToInstanced, config=Game,HideCategories=("Object"))
class MOTIONSYMPHONY_API UMotionBlendSpaceObject : public UMotionAnimObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	mutable UBlendSpace* BlendSpace;
	
	UPROPERTY(EditAnywhere, Category = "Blend Space")
	FVector2D SampleSpacing;

public:
	UMotionBlendSpaceObject(const FObjectInitializer& ObjectInitializer);
	void Initialize(const UMotionBlendSpaceObject* Copy);
	void Initialize(const int32 AnimId, TObjectPtr<UBlendSpace> InBlendSpace, TObjectPtr<UMotionDataAsset> InParentMotionData);
	virtual void InitializeFromLegacy(FMotionAnimAsset* LegacyMotion) override;
	
	virtual double GetPlayLength() const override;
	virtual double GetFrameRate() const override;
};

UCLASS(EditInlineNew, DefaultToInstanced, config=Game, HideCategories=("Object"))
class MOTIONSYMPHONY_API UMotionCompositeObject : public UMotionAnimObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	mutable UAnimComposite* AnimComposite;

public:
	UMotionCompositeObject(const FObjectInitializer& ObjectInitializer);
	void Initialize(const UMotionCompositeObject* Copy);
	void Initialize(const int32 AnimId,TObjectPtr<UAnimComposite> InComposite, TObjectPtr<UMotionDataAsset> InParentMotionData);

	virtual void InitializeFromLegacy(FMotionAnimAsset* LegacyMotion) override;
	
	virtual double GetPlayLength() const override;
	virtual double GetFrameRate() const override;

	virtual void GetRootBoneTransform(FTransform& OutTransform, const float Time, const bool bMirrored) const override;
	virtual void CacheTrajectoryPoints(TArray<FVector>& OutTrajectoryPoints, const bool bMirrored) const override;
};
