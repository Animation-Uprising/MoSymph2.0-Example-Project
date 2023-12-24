//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Data/MotionAnimAsset.h"
#include "MotionTag.generated.h"

//struct FMotionAnimAsset;
struct FPoseMotionData;

UCLASS(abstract, editinlinenew, Blueprintable, const, hidecategories=Object, collapsecategories, meta=(ShowWorldContextPin))
class MOTIONSYMPHONY_API UMotionTag : public UObject
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MotionTag)
	FColor TagColor;
#endif //WITH_EDITORONLY_DATA

public:
	UFUNCTION(BlueprintNativeEvent)
	FString GetTagName() const;

	UFUNCTION(BlueprintImplementableEvent)
	void Received_RunPreProcessForTag(/*FPoseMotionData& OutPose, FMotionAnimAsset& AnimAsset,*/ float PoseInterval) const;

#if WITH_EDITOR
	virtual void OnMotionTagCreatedInEditor() {};
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const { return true; }
#endif //WITH_EDITOR

	virtual void RunPreProcessForTag(/*FPoseMotionData& OutPose, FMotionAnimAsset& AnimAsset,*/ float PoseInterval);
	
	virtual FString GetEditorComment();

	virtual FLinearColor GetEditorColor();

	virtual void PostLoad();
};