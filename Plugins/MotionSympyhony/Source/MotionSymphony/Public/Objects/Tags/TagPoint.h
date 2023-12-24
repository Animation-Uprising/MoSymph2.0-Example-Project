//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Data/MotionAnimAsset.h"
#include "Objects/Assets/MotionDataAsset.h"
#include "Data/PoseMotionData.h"
#include "TagPoint.generated.h"

UCLASS(abstract, Blueprintable, const, hidecategories = (Object, TriggerSettings, Category))
class MOTIONSYMPHONY_API UTagPoint : public UAnimNotify
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintImplementableEvent)
	bool Received_PreProcessTag(UPARAM(ref)const FPoseMotionData& PointPose, UMotionAnimObject* OutMotionAnim,
	                            UMotionDataAsset* OutMotionData, const float Time) const;

	virtual void PreProcessTag(const FPoseMotionData& PointPose, TObjectPtr<UMotionAnimObject> OutMotionAnim,
	                           UMotionDataAsset* OutMotionData, const float Time);

public:
	virtual void CopyTagData(UTagPoint* CopyTag);
};