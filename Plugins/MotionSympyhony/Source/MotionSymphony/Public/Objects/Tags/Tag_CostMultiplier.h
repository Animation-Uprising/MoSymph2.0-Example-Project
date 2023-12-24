//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TagSection.h"
#include "Tag_CostMultiplier.generated.h"

UCLASS(editinlinenew, hidecategories = (Object, TriggerSettings, Category))
class MOTIONSYMPHONY_API UTag_CostMultiplier : public UTagSection
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "Tag")
	float CostMultiplier;
	
	UPROPERTY(EditAnywhere, Category = "Tag")
	bool bOverride;

public:
	virtual void PreProcessTag(TObjectPtr<UMotionAnimObject> OutMotionAnim, UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime) override;
	virtual void PreProcessPose(FPoseMotionData& OutPose, TObjectPtr<UMotionAnimObject> OutMotionAnim, UMotionDataAsset* OutMotionData, const float StartTime, const float EndTime) override;

	virtual void CopyTagData(UTagSection* CopyTag) override;
};