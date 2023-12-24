//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Data/MotionTag.h"
#include "Data/MotionAnimAsset.h"
#include "Data/PoseMotionData.h"

UMotionTag::UMotionTag(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	TagColor = FColor(200, 200, 255, 255);
#endif //WITH_EDITORONLY_DATA
}

FString UMotionTag::GetTagName_Implementation() const
{
	FString TagName;

#if WITH_EDITOR
	if (UObject* ClassGeneratedBy = GetClass()->ClassGeneratedBy)
	{
		TagName = ClassGeneratedBy->GetName();
	}
	else
#endif
	{
		TagName = GetClass()->GetName();
	}

	TagName = TagName.Replace(TEXT("MotionTag_"), TEXT(""), ESearchCase::CaseSensitive);

	return TagName;
}

void UMotionTag::RunPreProcessForTag(/*FPoseMotionData& OutPose, FMotionAnimAsset& AnimAsset,*/ float PoseInterval)
{
	Received_RunPreProcessForTag(/*OutPose, AnimAsset,*/ PoseInterval);
}

FString UMotionTag::GetEditorComment()
{
	return TEXT("Motion Matching Tag");
}

FLinearColor UMotionTag::GetEditorColor()
{
#if WITH_EDITORONLY_DATA
	return FLinearColor(TagColor);
#else
	return FLinearColor::Black;
#endif //WITH_EDITORONLY_DATA
}

void UMotionTag::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	//Ensure all loaded tags are transactional
	SetFlags(GetFlags() | RF_Transactional);
#endif
}

