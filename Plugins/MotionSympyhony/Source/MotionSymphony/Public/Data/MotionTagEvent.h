//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimLinkableElement.h"
#include "MotionTagEvent.generated.h"

USTRUCT(BlueprintType)
struct FMotionTagEvent : public FAnimLinkableElement
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = MotionTagEvent)
	FName TagName;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = MotionTagEvent)
	class UMotionTag* Tag;

	/** How long the tag lasts*/
	UPROPERTY()
	float Duration;

	/** Linkable element to use for the end handle representing a tag duration*/
	UPROPERTY()
	FAnimLinkableElement EndLink;

#if WITH_EDITORONLY_DATA
	/** Color of tag in editor*/
	UPROPERTY()
	FColor TagColor;

	/** Guide for tracking tags in editor */
	UPROPERTY()
	FGuid Guid;
#endif //WITH_EDITORONLY_DATA

	/** Track that the tag exists on, used for visual placement in editor and sorting priority in runtime */
	UPROPERTY()
	int32 TrackIndex;

private:
	/** TagName appended to "MotionTag_" */
	mutable FName CachedTagEventName;

	mutable FName CachedTagEventBaseName;

public:
	FMotionTagEvent();
	virtual ~FMotionTagEvent();

#if WITH_EDITORONLY_DATA
	bool Serialize(FArchive& Ar);
	void PostSerialize(const FArchive& Ar);
#endif

	/** Returns the trigger time for this tag.*/
	float GetTriggerTime() const;

	/** Returns the end trigger time for this tag*/
	float GetEndTriggerTime() const;

	/** Returns the length of the tag */
	float GetDuration() const;

	/** Sets the current lenght of the tag */
	float SetDuration(float NewDuration);

	/** Returns true if this is a blueprint derived tag */
	bool IsBlueprintTag() const;

	bool operator ==(const FMotionTagEvent& Other) const;
	bool operator <(const FMotionTagEvent& Other) const;

	virtual void SetTime(float NewTime, EAnimLinkMethod::Type ReferenceFrame = EAnimLinkMethod::Absolute) override;
	FName GetTagEventName() const;
};

//#if WITH_EDITORONLY_DATA
//	template<> struct TStructOpsTypeTraits<FMotionTagEvent : public TStructOpsTypeTraitsBase2<FMotionTagEvent>
//	{
//		enum
//		{
//			WithSerializer = true,
//			WithPostSerializer = true
//		};
//	}
//#endif