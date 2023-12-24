//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#include "Tags/Tag_CostMultiplier.h"

UTag_CostMultiplier::UTag_CostMultiplier(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	CostMultiplier(1.0f),
	bOverride(true)
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor::Purple;
#endif
}


void UTag_CostMultiplier::PreProcessTag(TObjectPtr<UMotionAnimObject> OutMotionAnim,
                                        UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime) 
{
	Super::PreProcessTag(OutMotionAnim, OutMotionData, StartTime, EndTime);
}

void UTag_CostMultiplier::PreProcessPose(FPoseMotionData& OutPose, TObjectPtr<UMotionAnimObject> OutMotionAnim,
                                         UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime)
{
	Super::PreProcessPose(OutPose, OutMotionAnim, OutMotionData, StartTime, EndTime);

	if (CostMultiplier < 0.1f)
	{
		UE_LOG(LogTemp, Warning, TEXT("CostMultiplier Tag has a value less than 0.1f? Is this intended? It may negatively impact runtime motion matching"));
	}

	//Note: The first atom in each pose within the matrix is used for PoseFavour instead of cost distance calculations
	FPoseMatrix& LookupPoseMatrix = OutMotionData->LookupPoseMatrix;
	if (bOverride)
	{
		LookupPoseMatrix.PoseArray[OutPose.PoseId * LookupPoseMatrix.AtomCount] = FMath::Abs(CostMultiplier);
	}
	else
	{
		LookupPoseMatrix.PoseArray[OutPose.PoseId * LookupPoseMatrix.AtomCount] *= FMath::Abs(CostMultiplier);
	}
}

void UTag_CostMultiplier::CopyTagData(UTagSection* CopyTag)
{
	if(const UTag_CostMultiplier* Tag = Cast<UTag_CostMultiplier>(CopyTag))
	{
		CostMultiplier = Tag->CostMultiplier;
		bOverride = Tag->bOverride;
	}
}
