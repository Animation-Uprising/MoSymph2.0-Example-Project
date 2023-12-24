//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNode_PoseMatchBase.h"
#include "AnimNode_PoseMatching.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct MOTIONSYMPHONY_API FAnimNode_PoseMatching : public FAnimNode_PoseMatchBase
{
	GENERATED_BODY()
public:
	FAnimNode_PoseMatching();

	virtual UAnimSequenceBase* FindActiveAnim() override;
	virtual void PreProcess() override;
};