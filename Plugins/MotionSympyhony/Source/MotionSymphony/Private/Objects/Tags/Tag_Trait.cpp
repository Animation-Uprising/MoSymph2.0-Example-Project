//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Tags/Tag_Trait.h"
#include "Utility/MMBlueprintFunctionLibrary.h"
#include "MotionSymphonySettings.h"

UTag_Trait::UTag_Trait(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor::Yellow;
#endif
}

void UTag_Trait::PreProcessTag(TObjectPtr<UMotionAnimObject> OutMotionAnim,
                               UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime) 
{
	Super::PreProcessTag(OutMotionAnim, OutMotionData, StartTime, EndTime);
}


void UTag_Trait::PreProcessPose(FPoseMotionData& OutPose, TObjectPtr<UMotionAnimObject> OutMotionAnim,
                                UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime)
{
	Super::PreProcessPose(OutPose, OutMotionAnim, OutMotionData, StartTime, EndTime);
	
	OutPose.MotionTags.AddTag(MotionTags);
}

void UTag_Trait::CopyTagData(UTagSection* CopyTag)
{
	if(UTag_Trait* Tag = Cast<UTag_Trait>(CopyTag))
	{
		MotionTags = Tag->MotionTags;
	}
}
