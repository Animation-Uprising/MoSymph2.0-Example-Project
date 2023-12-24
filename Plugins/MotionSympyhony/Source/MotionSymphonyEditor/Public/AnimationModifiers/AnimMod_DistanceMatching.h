//Copyright 2020-2023 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimationModifier.h"
#include "AnimMod_DistanceMatching.generated.h"

class UAnimSequence;

/** Animation modifier for generating a distance matching curve to a point within the animation 
The animation point must be set by playing an animNotify and calling it 'DistanceMatch' */
UCLASS()
class MOTIONSYMPHONYEDITOR_API UAnimMod_DistanceMatching : public UAnimationModifier
{
	GENERATED_BODY()
	
	virtual void OnApply_Implementation(UAnimSequence* AnimationSequence) override;
	virtual void OnRevert_Implementation(UAnimSequence* AnimationSequence) override;
};
